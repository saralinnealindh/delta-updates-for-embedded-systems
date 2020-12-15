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

//DEFINE LED BLINKING SPEED 
#define SLEEP_TIME_MS   1000

//CONFIG LED 
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

//CONFIG BUTTON
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

// IMAGE OFFSETS
#define PRIMARY_OFFSET FLASH_AREA_OFFSET(image_0)
#define PRIMARY_SIZE FLASH_AREA_SIZE(image_0)
#define SECONDARY_OFFSET FLASH_AREA_OFFSET(image_1)
#define SECONDARY_SIZE FLASH_AREA_SIZE(image_1)
#define STORAGE_OFFSET FLASH_AREA_OFFSET(storage)
#define STORAGE_SIZE FLASH_AREA_SIZE(storage)

// PATCH HEADER SIZE
#define HEADER_SIZE 0x18

// FLASH MEMORY AND POINTERS TO CURRENT LOCATION OF BUFFER AND END OF IMAGE AREA 
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

//Responds to button 1 being pressed 
void button_pressed(const struct device *dev, 
					struct gpio_callback *cb,
		    		uint32_t pins);

/**
 * Functiong for initiating the devices used by the 
 * program. 
 *
 * @param[in] flash the flash memory. 
 * @param[in] button the button.
 * @param[in] led the led.
 *
 * @return zero(0) or error code.
 */
static int configure_devices(const struct device *flash, 
					const struct device *button, 
					const struct device *led);

/**
 * Functiong for writing to secondary partition.
 *
 * @param[in] arg_p pointer to the devices flash memory. 
 * @param[in] buf_p buffer containing information to be written.
 * @param[in] size size of the buffer.
 *
 * @return zero(0) or error code.
 */
static int du_flash_write(void *arg_p,
					const uint8_t *buf_p,
					size_t size);

/**
 * Functiong for reading from primary partition.
 *
 * @param[in] arg_p pointer to the devices flash memory. 
 * @param[in] buf_p buffer to be read into.
 * @param[in] size size of the buffer.
 *
 * @return zero(0) or error code.
 */
static int du_flash_from_read(void *arg_p,
					uint8_t *buf_p,
					size_t size);

/**
 * Functiong for reading from patch partition.
 *
 * @param[in] arg_p pointer to the devices flash memory. 
 * @param[in] buf_p buffer to be read into.
 * @param[in] size size of the buffer.
 *
 * @return zero(0) or error code.
 */
static int du_flash_patch_read(void *arg_p,
					uint8_t *buf_p,
					size_t size);

/**
 * Functiong for jumping on the primary partition
 *
 * @param[in] arg_p pointer to the devices flash memory. 
 * @param[in] offset the size of the jump forward in bytes.
 *
 * @return zero(0) or error code.
 */
static int du_flash_seek(void *arg_p,
					int offset);

/**
 * Functiong for erasing data on the entire secondary partiton.
 *
 * @param[in] flash the devices flash memory. 
 *
 * @return zero(0) or error code.
 */
static int erase_secondary(const struct device *flash);

/**
 * Functiong initiating the flash_mem struct by setting
 * all offsets to their correct adresses.
 *
 * @param[in] flash the devices flash memory. 
 *
 * @return zero(0) or error code.
 */
static int init_flash_mem(struct flash_mem *flash);

/**
 * Functiong for reading the metadata from the patch and 
 * changing the header to mark that the patch has been 
 * applied.
 *
 * @param[in] flash the devices flash memory. 
 *
 * @return zero(0) or error code.
 */
static int read_patch_header(struct flash_mem *flash);