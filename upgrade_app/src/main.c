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
#include "detools/detools.h"


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

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
#define PRIMARY_SIZE FLASH_AREA_SIZE(image_0)
#define SECONDARY_OFFSET FLASH_AREA_OFFSET(image_1)
#define SECONDARY_SIZE FLASH_AREA_SIZE(image_1)
#define STORAGE_OFFSET FLASH_AREA_OFFSET(storage)
#define STORAGE_SIZE FLASH_AREA_SIZE(storage)

// HEADER SIZE
#define HEADER_SIZE 24

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

static int erase_secondary(const struct device *flash);

static int init_flash_mem(struct flash_mem *flash);

static int read_patch_header(struct flash_mem *flash);

/* MAIN FUNCTION */

void main(void)
{
	const struct device *led,*button;
	struct flash_mem *flash_pt;
	int ret,length,patch_size;
	bool led_is_on;

	led = device_get_binding(LED0);
	button = device_get_binding(SW0_GPIO_LABEL);
	flash_pt = k_malloc(sizeof(struct flash_mem));
	flash_pt->device = device_get_binding(DT_CHOSEN_ZEPHYR_FLASH_CONTROLLER_LABEL);

	length = 8;
	led_is_on = true;
	btn_flag = false; 

	unsigned char buf[length];

	if (!led||!button||!flash_pt->device)
		return;

	if (configure_devices(flash_pt->device,button,led))
		return;

	if(init_flash_mem(flash_pt))
		return;
	
	printk("Hello World from %s on %s!\n",MCUBOOT_BLINKY2_FROM, CONFIG_BOARD);

	ret = erase_secondary(flash_pt->device);

	/*ret = test_callbacks(du_flash_from_read,
						du_flash_seek,
						du_flash_patch_read,
						512,
						du_flash_write,
						flash_pt);*/

	if (ret) 
		printk("Something went wrong :( %X \n",ret);

	/*Main loop*/
	while (1) {
		gpio_pin_set(led, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(SLEEP_TIME_MS);

		if(btn_flag)
		{
			ret = flash_read(flash_pt->device,SECONDARY_OFFSET, buf,length);

			if(ret)
				return;

			patch_size = read_patch_header(flash_pt);

			if(patch_size>0) //if patch exists
			{
				printk("Appling patch... \n");
				ret = detools_apply_patch_callbacks(du_flash_from_read,
											du_flash_seek,
											du_flash_patch_read,
											patch_size,
											du_flash_write,
											flash_pt);
				if(ret<=0)
					printk("Error: %x \n",ret);
			}

			printk("Size: %X\n",patch_size);
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
	size_t size = 8;
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
		return 0x18;//couldnt convert to flash_mem

	if (size%4) 
		return 0x12; //if word not aligned

	if (flash_write_protection_set(flash->device, false)) 
		return 0x13; //write protection could not be removed

	if (flash_write(flash->device, flash->to_current, buf_p, size)) 
		return 0x14; //writing could not be done

	if (flash_write_protection_set(flash->device, true))
		return 0x15; //writing protection could not be set again

	flash->to_current += (off_t) size;

	if (flash->to_current>=flash->to_end)
		return 0x1; //out of memory

	return 0;

}

static int du_flash_from_read(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p; 

	if(!flash)
		return 0x28; //couldnt convert to flash_mem

	if (size <= 0) 
		return 0x26; //invalid buff size

	if(flash_read(flash->device,flash->from_current,buf_p,size))
		return 0x27; //could not read

	flash->from_current += (off_t) size;

	if (flash->from_current>=flash->from_end)
		return 0x21; //out of memory

	return 0;
}

static int du_flash_patch_read(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p; 

	if(!flash) 
		return 0x38; //couldnt convert to flash_mem

	if (size <= 0) 
		return 0x36; //invalid buff size

	if (flash_read(flash->device,flash->patch_current,buf_p,size))
		return 0x37; //could not read 
	
	flash->patch_current += (off_t) size;

	if (flash->patch_current>=flash->patch_end)
		return 0x31; //out of memory
	
	return 0;
}

static int du_flash_seek(void *arg_p,int offset)
{
	struct flash_mem *flash;
    flash = (struct flash_mem *)arg_p;

	if(!flash)
		return 0x48; //couldnt convert to flash_mem

	flash->from_current += offset;

	if (flash->from_current>=flash->from_end)
		return 0x41; //out of memory

	return 0;

}

static int init_flash_mem(struct flash_mem *flash)
{
	if(!flash)
		return -1;

	flash->from_current = PRIMARY_OFFSET;
	flash->from_end = flash->from_current + PRIMARY_SIZE;

	flash->to_current = SECONDARY_OFFSET;
	flash->to_end = flash->to_current + SECONDARY_SIZE;

	flash->patch_current = STORAGE_OFFSET;
	//flash->patch_current = STORAGE_OFFSET +0x8;
	flash->patch_end = flash->patch_current + STORAGE_SIZE;

	return 0;
}

static int erase_secondary(const struct device *flash) 
{
	if (flash_write_protection_set(flash, false)) 
		return 0x53; //write protection could not be removed

	if (flash_erase(flash,SECONDARY_OFFSET,SECONDARY_SIZE)) 
		return 0x54; //writing could not be done

	if (flash_write_protection_set(flash, true))
		return 0x55; //writing protection could not be set again

	return 0;
}

static int read_patch_header(struct flash_mem *flash)
{
	int header_len, word_len, size;
	word_len = 8;
	header_len = HEADER_SIZE;
	unsigned char *buf_p,*buf_end,buf[header_len];
	unsigned char word_buf[] = {'N','E','W','P','A','T','C','H'};
	unsigned char new_word[] = {'F','F','F','F','F','F','F','F'};

	if(flash_read(flash->device,STORAGE_OFFSET,buf,header_len))
		return -0x7; //could not read flash

	if(!strncmp(buf,word_buf,word_len))
		return -1; //no new patch
	
	buf_p=&buf[word_len];
	buf_end=&buf[header_len];

	size = (int) strtol(buf_p,buf_end,header_len-word_len);

	if (flash_write_protection_set(flash->device, false)) 
		return -0x3; //write protection could not be removed

	if (flash_write(flash->device, STORAGE_OFFSET,new_word, word_len)) 
		return -0x4; //writing could not be done

	if (flash_write_protection_set(flash->device, true))
		return -0x5; //writing protection could not be set again

	return size;
}





