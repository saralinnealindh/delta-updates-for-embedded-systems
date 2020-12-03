/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/flash.h>
#include <inttypes.h>
#include <storage/flash_map.h>
#include <stdio.h>


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
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

/*
 * Get button configuration from the devicetree sw0 alias.
 *
 * At least a GPIO device and pin number must be provided. The 'flags'
 * cell is optional.
 */

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

// STORAGE 
#define STORAGE_OFFSET FLASH_AREA_OFFSET(storage)

static struct gpio_callback button_cb_data;
static bool btn_flag; 

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins);

bool configure_devices(const struct device *flash, const struct device *button, const struct device *led);


void main(void)
{
	const struct device *led,*button,*flash;
	int ret,length;
	bool led_is_on;

	led = device_get_binding(LED0);
	button = device_get_binding(SW0_GPIO_LABEL);
	flash = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);

	length = 8;
	led_is_on = true;
	btn_flag = false; 

	unsigned char buf[length];

	if (!led||!button||!flash)
		return;

	if (!configure_devices(flash,button,led))
		return;
	
	printk("Hello World from %s on %s!\n",MCUBOOT_BLINKY2_FROM, CONFIG_BOARD);

	/*Main loop*/
	while (1) {
		gpio_pin_set(led, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(SLEEP_TIME_MS);

		if(btn_flag){
			// Read from memory
			if(!flash){
				return;
			}

			ret = flash_read(flash,STORAGE_OFFSET, &buf,(length+1)*sizeof(char));

			if(ret!=0){
				return;
			}

			printk("Word: %s\n",buf);

			btn_flag = false;
		}
	}
}

/*Exectues when button is pressed*/
void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	btn_flag = true;
}

bool configure_devices(const struct device *flash, const struct device *button, const struct device *led) {
	int ret; 
	ret = gpio_pin_configure(led, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	ret |= gpio_pin_configure(button, SW0_GPIO_PIN, SW0_GPIO_FLAGS);

	if (ret!=0)
		return false;

	ret = gpio_pin_interrupt_configure(button,SW0_GPIO_PIN,GPIO_INT_EDGE_TO_ACTIVE);

	if (ret!=0)
		return false;
	
	gpio_init_callback(&button_cb_data, button_pressed, BIT(SW0_GPIO_PIN));
	gpio_add_callback(button, &button_cb_data);

	return true; 
}
