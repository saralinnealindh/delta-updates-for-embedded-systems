#ifndef DELTA_H
#define DELTA_H

#include <zephyr.h>
#include <sys/util.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <dfu/mcuboot.h>
#include <power/reboot.h>
#include "../detools/detools.h"

/* IMAGE OFFSETS AND SIZES */
#define PRIMARY_OFFSET FLASH_AREA_OFFSET(image_0)
#define PRIMARY_SIZE FLASH_AREA_SIZE(image_0)
#define SECONDARY_OFFSET FLASH_AREA_OFFSET(image_1)
#define SECONDARY_SIZE FLASH_AREA_SIZE(image_1)
#define STORAGE_OFFSET FLASH_AREA_OFFSET(storage)
#define STORAGE_SIZE FLASH_AREA_SIZE(storage)

/* PATCH HEADER SIZE */
#define HEADER_SIZE 0x18

/* PAGE SIZE */
#define PAGE_SIZE 0x1000

/* Error codes. */
#define DELTA_OK                                          0
#define DELTA_SLOT1_OUT_OF_MEMORY                        28
#define DELTA_READING_PATCH_ERROR						 29
#define DELTA_READING_SOURCE_ERROR						 30
#define DELTA_WRITING_ERROR			                     31
#define DELTA_SEEKING_ERROR								 32
#define DELTA_CASTING_ERROR                              33
#define DELTA_INVALID_BUF_SIZE							 34
#define DELTA_CLEARING_ERROR							 35
#define DELTA_NO_FLASH_FOUND							 36
#define DELTA_PATCH_HEADER_ERROR                         37

/* FLASH MEMORY AND POINTERS TO CURRENT LOCATION OF BUFFERS AND END OF IMAGE AREAS.
 * - "Patch" refers to the area containing the patch image. 
 * - "From" refers to the area containing the source image. 
 * - "To" refers to the area where the target image is to be placed.
 * */
struct flash_mem {
	const struct device *device;
	off_t patch_current; 
	off_t patch_end; 
	off_t from_current; 
	off_t from_end; 
	off_t to_current; 
	off_t to_end;
	size_t write_buf; 
};

/* FUNCTION DECLARATIONS */

/**
 * Checks if there is patch in the patch partition 
 * and applies that patch if it exists. Then restarts
 * the device and boots from the new image. 
 *
 * @param[in] flash the devices flash memory. 
 *
 * @return zero(0) if no patch or a negative error
 * code.
 */
int delta_check_and_apply(struct flash_mem *flash);

/**
 * Functiong for reading the metadata from the patch and 
 * changing the header to mark that the patch has been 
 * applied.
 *
 * @param[in] flash the devices flash memory. 
 *
 * @return zero(0) if not patch, a negative value (0>)
 * representing an error, or a positive value (0<)
 * representing the patch size. 
 */
int delta_read_patch_header(struct flash_mem *flash);

/**
 * Get the error string for given error code.
 *
 * @param[in] Error code.
 *
 * @return Error string.
 */
const char *delta_error_as_string(int error);

#endif