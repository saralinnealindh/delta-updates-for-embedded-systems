/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include "delta/delta.h"

/* BUTTON */
#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});

static struct gpio_callback button_cb_data;
static bool btn_flag;

/* LED */
#define SLEEP_TIME_MS   1000 /*BLINKING SPEED*/

#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct device *flash_device =
	DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_flash_controller));

/* Debug printouts (adds about 800 B to img)
 * Define to 0 to disable
 */
#ifndef PRINT_ERRORS
	#define PRINT_ERRORS 1
#endif

/*Initiating button and led*/
static int config_devices(void);

/*Responds to button 1 being pressed*/
void button_pressed(const struct device *dev,
					struct gpio_callback *cb,
					uint32_t pins);

/* MAIN FUNCTION */
void main(void)
{
	struct flash_mem *flash_pt;
	int ret;

	flash_pt = k_malloc(sizeof(struct flash_mem));
	flash_pt->device = flash_device;

	if (!flash_pt->device) {
		return;
	}
	if (config_devices()) {
		return;
	}

	/*Main loop*/
	while (1) {
		/* turn light on/off */
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return;
		}
		k_msleep(SLEEP_TIME_MS);

		if (btn_flag) {
			ret = delta_check_and_apply(flash_pt);
			if (ret) {
				#if PRINT_ERRORS == 1
				printk("%s", delta_error_as_string(ret));
				#endif
				return;
			}
			btn_flag = false;
		}
	}
}

static int config_devices(void)
{
	int ret;

	/* setup led */
	if (!gpio_is_ready_dt(&led)) {
		return -1;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return -1;
	}

	/*setup button*/
	if (!gpio_is_ready_dt(&button)) {
		return -1;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret) {
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret) {
		return -1;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	btn_flag = false;

	return 0;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	btn_flag = true;
}
