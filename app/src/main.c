#include <zephyr.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <drivers/gpio.h>
#include "delta/delta.h"

/* BUTTON */
#define SW0_NODE	DT_ALIAS(sw0)

#if DT_NODE_HAS_STATUS(SW0_NODE, okay)
#define SW0_GPIO_LABEL	DT_GPIO_LABEL(SW0_NODE, gpios)
#define SW0_GPIO_PIN	DT_GPIO_PIN(SW0_NODE, gpios)
#define SW0_GPIO_FLAGS	(GPIO_INPUT | DT_GPIO_FLAGS(SW0_NODE, gpios))
#else
#error "Unsupported board: sw0 devicetree alias is not defined"
#define SW0_GPIO_LABEL	""
#define SW0_GPIO_PIN	0
#define SW0_GPIO_FLAGS	0
#endif

static struct gpio_callback button_cb_data;
static bool btn_flag;

/* LED */
#define SLEEP_TIME_MS   1000 //BLINKING SPEED

#define LED0_NODE DT_ALIAS(led1)
#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
#define LED0	DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN	DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LED0_NODE, gpios)
#else
/* A build error here means your board isn't set up to blink an LED. */
#error "Unsupported board: led0 devicetree alias is not defined"
#define LED0	""
#define PIN	0
#define FLAGS	0
#endif

/* Debug printouts (adds about 800 B to img) 
 * Define to 0 to disable */
#ifndef PRINT_ERRORS
	#define PRINT_ERRORS 1
#endif

//Initiating button and led
static int config_devices(const struct device *button, 
					const struct device *led);

//Responds to button 1 being pressed 
void button_pressed(const struct device *dev, 
					struct gpio_callback *cb,
		    		uint32_t pins);

/* MAIN FUNCTION */
void main(void)
{
	const struct device *led,*button;
	struct flash_mem *flash_pt;
	bool led_is_on = true;
	int ret;

	led = device_get_binding(LED0);
	button = device_get_binding(SW0_GPIO_LABEL);
	flash_pt = k_malloc(sizeof(struct flash_mem));
	flash_pt->device = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);

	if (!led||!button||!flash_pt->device)
		return;

	if (config_devices(button,led))
		return;
	
	/*Main loop*/
	while (1) 
	{
		/* turn light on/off */
		gpio_pin_set(led, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(SLEEP_TIME_MS);

		if(btn_flag)
		{
			ret = delta_check_and_apply(flash_pt);

			if(ret){
				#if PRINT_ERRORS == 1
				printk("%s",delta_error_as_string(ret));
				#endif
				return;
			}
				
			btn_flag = false;
		}
	}
}

static int config_devices(const struct device *button, const struct device *led)
{
	int ret; 
	ret = gpio_pin_configure(led, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	ret |= gpio_pin_configure(button, SW0_GPIO_PIN, SW0_GPIO_FLAGS);

	if (ret)
		return -1;

	if (gpio_pin_interrupt_configure(button,SW0_GPIO_PIN,GPIO_INT_EDGE_TO_ACTIVE))
		return -1;
	
	gpio_init_callback(&button_cb_data, button_pressed, BIT(SW0_GPIO_PIN));
	gpio_add_callback(button, &button_cb_data);

	btn_flag = false; 

	return 0; 
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	btn_flag = true;
}