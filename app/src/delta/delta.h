/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DELTA_H
#define DELTA_H

#include <zephyr/sys/util.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>
#include "../detools/detools.h"
#include <zephyr/logging/log.h>

/* IMAGE OFFSETS AND SIZES */
#define PRIMARY_OFFSET FIXED_PARTITION_OFFSET(slot0_partition)
#define PRIMARY_SIZE FIXED_PARTITION_SIZE(slot0_partition)
#define SECONDARY_OFFSET FIXED_PARTITION_OFFSET(slot1_partition)
#define SECONDARY_SIZE FIXED_PARTITION_SIZE(slot1_partition)
#define STORAGE_OFFSET FIXED_PARTITION_OFFSET(storage_partition)
#define STORAGE_SIZE FIXED_PARTITION_SIZE(storage_partition)

/* PATCH HEADER SIZE */
#define HEADER_SIZE 0x8

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
 */
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
int delta_read_patch_header(struct flash_mem *flash, uint32_t *size_ptr);

/**
 * Get the error string for given error code.
 *
 * @param[in] Error code.
 *
 * @return Error string.
 */
const char *delta_error_as_string(int error);

#endif