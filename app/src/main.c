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
#include "detools.h"


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

// IMAGE OFFSETS
#define PRIMARY_OFFSET FLASH_AREA_OFFSET(image_0)
#define SECONDARY_OFFSET FLASH_AREA_OFFSET(image_1)
#define SCRATCH_OFFSET FLASH_AREA_OFFSET(image_scratch)
#define STORAGE_OFFSET FLASH_AREA_OFFSET(storage)

static struct gpio_callback button_cb_data;
static bool btn_flag;

// STRUCTS 
struct flash_mem {
	const struct device *device;
	off_t patch_current; //current patch offset
	off_t patch_end; //end of patch partition
	off_t from_current; //current from file offset
	off_t from_end; //end of primary partition
	off_t to_current; //current from file offset
	off_t to_end; //end of primary partition
};

/* FUNCTION DECLARATIONS */

void button_pressed(const struct device *dev, 
					struct gpio_callback *cb,
		    		uint32_t pins);

static int configure_devices(const struct device *flash, 
					const struct device *button, 
					const struct device *led);

int test_callbacks(detools_read_t from_read,
					detools_seek_t from_seek,
					detools_read_t patch_read,
					size_t patch_size,
					detools_write_t to_write,
					void *arg_p);

static int du_flash_write(void *arg_p,
					const uint8_t *buf_p,
					size_t size);

static int du_flash_from_read(void *arg_p,
					const uint8_t *buf_p,
					size_t size);

static int du_flash_patch_read(void *arg_p,
					const uint8_t *buf_p,
					size_t size);

static int du_flash_seek(void *arg_p,
					int offset);

static int init_flash_mem(struct flash_mem *flash, const struct device *flash_dev);

/* MAIN FUNCTION */

void main(void)
{
	const struct device *led,*button,*flash;
	struct flash_mem *flash_mem;
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

	if (configure_devices(flash,button,led))
		return;

	if(init_flash_mem(flash_mem,flash))
		return;
	
	printk("Hello World from %s on %s!\n",MCUBOOT_BLINKY2_FROM, CONFIG_BOARD);

	ret = test_callbacks(du_flash_from_read,
						du_flash_seek,
						du_flash_patch_read,
						512,
						du_flash_write,
						flash_mem);

	if (ret) 
		printk("Something went wrong :( %X \n",ret);

	/*Main loop*/
	while (1) {
		gpio_pin_set(led, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(SLEEP_TIME_MS);

		if(btn_flag){
			ret = flash_read(flash,SECONDARY_OFFSET, &buf,length+1);

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

static int configure_devices(const struct device *flash, const struct device *button, const struct device *led)
{
	int ret; 
	ret = gpio_pin_configure(led, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	ret |= gpio_pin_configure(button, SW0_GPIO_PIN, SW0_GPIO_FLAGS);

	if (ret)
		return -1;

	ret = gpio_pin_interrupt_configure(button,SW0_GPIO_PIN,GPIO_INT_EDGE_TO_ACTIVE);

	if (ret)
		return -1;
	
	gpio_init_callback(&button_cb_data, button_pressed, BIT(SW0_GPIO_PIN));
	gpio_add_callback(button, &button_cb_data);

	return 0; 
}

int test_callbacks(detools_read_t from_read,
					detools_seek_t from_seek,
					detools_read_t patch_read,
					size_t patch_size,
					detools_write_t to_write,
					void *arg_p) 
{
	size_t size = 4;
	uint8_t buf[size];
	int ret; 

	ret = patch_read(arg_p,buf,size);

	if(ret)
		return ret;

	ret = to_write(arg_p,buf,size);

	if(ret)
		return ret; 
	
	return 0;
}

static int du_flash_write(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p;

	if(!flash)
		return -0x8; //couldnt convert to flash_mem

	if (size%4) 
		return -0x2; //if word not aligned

	if (flash_write_protection_set(flash->device, false)) 
		return -0x3; //write protection could not be set

	if (flash_write(flash->device, flash->to_current, buf_p, size)) 
		return -0x4; //writing could not be done

	if (flash_write_protection_set(flash->device, true))
		return -0x5; //writing protection could not be set again

	flash->to_current += (off_t) size;

	if (flash->to_current>=flash->to_end)
		return -1; //out of memory

	return 0;

}

static int du_flash_from_read(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p; 

	if(!flash)
		return -0x8; //couldnt convert to flash_mem

	if (size <= 0) 
		return -0x6; //invalid buff size

	if(flash_read(flash->device,flash->from_current,buf_p,size))
		return -0x7; //could not read

	flash->from_current += (off_t) size;

	if (flash->from_current>=flash->from_end)
		return -1; //out of memory

	return 0;
}

static int du_flash_patch_read(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p; 

	if(!flash)
		return -0x8; //couldnt convert to flash_mem

	if (size <= 0) 
		return -0x6; //invalid buff size

	if (flash_read(flash->device,flash->patch_current,buf_p,size))
		return -0x7; //could not read 
	
	flash->patch_current += (off_t) size;

	if (flash->patch_current>=flash->patch_end)
		return -1; //out of memory
	
	return 0;
}

static int du_flash_seek(void *arg_p,
					int offset)

{
	struct flash_mem *flash;
    flash = (struct flash_mem *)arg_p;

	if(!flash)
		return -0x8; //couldnt convert to flash_mem

	flash->from_current += offset;

	if (flash->from_current>=flash->from_end)
		return -1; //out of memory

	return 0;

}

static int init_flash_mem(struct flash_mem *flash, const struct device *flash_dev) 
{
	flash->device = flash_dev;

	flash->from_current = PRIMARY_OFFSET;
	flash->from_end = SECONDARY_OFFSET-1;

	flash->patch_current = STORAGE_OFFSET +0x8;
	flash->patch_end = STORAGE_OFFSET+0x8000;

	flash->to_current = SECONDARY_OFFSET;
	flash->to_end = SCRATCH_OFFSET-1;
}





