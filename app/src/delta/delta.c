/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "delta.h"

/*
 *  IMAGE/FLASH MANAGEMENT
 */

static int erase_page(struct flash_mem *flash, off_t offset)
{
	offset = offset - offset%PAGE_SIZE; /* find start of page */

	if (flash_write_protection_set(flash->device, false)) {
		return -DELTA_CLEARING_ERROR;
	}
	if (flash_erase(flash->device, offset, PAGE_SIZE)) {
		return -DELTA_CLEARING_ERROR;
	}
	if (flash_write_protection_set(flash->device, true)) {
		return -DELTA_CLEARING_ERROR;
	}

	return DELTA_OK;
}

static int delta_flash_write(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash;

	flash = (struct flash_mem *)arg_p;
	flash->write_buf += size;

	if (flash->write_buf >= PAGE_SIZE) {
		if (erase_page(flash, flash->to_current + (off_t) size)) {
			return -DELTA_CLEARING_ERROR;
		}
		flash->write_buf = 0;
	}

	if (!flash) {
		return -DELTA_CASTING_ERROR;
	}
	if (flash_write_protection_set(flash->device, false)) {
		return -DELTA_WRITING_ERROR;
	}
	if (flash_write(flash->device, flash->to_current, buf_p, size)) {
		return -DELTA_WRITING_ERROR;
	}
	if (flash_write_protection_set(flash->device, true)) {
		return -DELTA_WRITING_ERROR;
	}

	flash->to_current += (off_t) size;
	if (flash->to_current >= flash->to_end) {
		return -DELTA_SLOT1_OUT_OF_MEMORY;
	}

	return DELTA_OK;
}

static int delta_flash_from_read(void *arg_p,
					uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash;

	flash = (struct flash_mem *)arg_p;

	if (!flash) {
		return -DELTA_CASTING_ERROR;
	}
	if (size <= 0) {
		return -DELTA_INVALID_BUF_SIZE;
	}

	if (flash_read(flash->device, flash->from_current, buf_p, size)) {
		return -DELTA_READING_SOURCE_ERROR;
	}

	flash->from_current += (off_t) size;
	if (flash->from_current >= flash->from_end) {
		return -DELTA_READING_SOURCE_ERROR;
	}

	return DELTA_OK;
}

static int delta_flash_patch_read(void *arg_p,
					uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash;

	flash = (struct flash_mem *)arg_p;

	if (!flash) {
		return -DELTA_CASTING_ERROR;
	}
	if (size <= 0) {
		return -DELTA_INVALID_BUF_SIZE;
	}

	if (flash_read(flash->device, flash->patch_current, buf_p, size)) {
		return -DELTA_READING_PATCH_ERROR;
	}

	flash->patch_current += (off_t) size;
	if (flash->patch_current >= flash->patch_end) {
		return -DELTA_READING_PATCH_ERROR;
	}

	return DELTA_OK;
}

static int delta_flash_seek(void *arg_p, int offset)
{
	struct flash_mem *flash;

	flash = (struct flash_mem *)arg_p;

	if (!flash) {
		return -DELTA_CASTING_ERROR;
	}

	flash->from_current += offset;

	if (flash->from_current >= flash->from_end) {
		return -DELTA_SEEKING_ERROR;
	}

	return DELTA_OK;
}

/*
 *  INIT
 */

static int delta_init_flash_mem(struct flash_mem *flash)
{
	if (!flash) {
		return -DELTA_NO_FLASH_FOUND;
	}

	flash->from_current = PRIMARY_OFFSET;
	flash->from_end = flash->from_current + PRIMARY_SIZE;

	flash->to_current = SECONDARY_OFFSET;
	flash->to_end = flash->to_current + SECONDARY_SIZE;

	flash->patch_current = STORAGE_OFFSET + HEADER_SIZE;
	flash->patch_end = flash->patch_current + STORAGE_SIZE;

	flash->write_buf = 0;

	return DELTA_OK;
}

static int delta_init(struct flash_mem *flash)
{
	int ret;

	ret = delta_init_flash_mem(flash);
	if (ret) {
		return ret;
	}
	ret = erase_page(flash, flash->to_current);
	if (ret) {
		return ret;
	}

	return DELTA_OK;
}

/*
 *  PUBLIC FUNCTIONS
 */

int delta_check_and_apply(struct flash_mem *flash)
{
	uint32_t patch_size; 
	int ret;

	ret = delta_read_patch_header(flash,&patch_size);

	if (ret < 0) {
		return ret;
	} else if (patch_size > 0) {
		ret = delta_init(flash);
		if (ret) {
			return ret;
		}
		ret = detools_apply_patch_callbacks(delta_flash_from_read,
											delta_flash_seek,
											delta_flash_patch_read,
											(size_t) patch_size,
											delta_flash_write,
											flash);
		if (ret <= 0) {
			return ret;
		}
		if (boot_request_upgrade(BOOT_UPGRADE_PERMANENT)) {
			return -1;
		}
		sys_reboot(SYS_REBOOT_COLD);
	}

	return DELTA_OK;
}

int delta_read_patch_header(struct flash_mem *flash, uint32_t size)
{
	uint32_t new_patch, reset_msg, patch_header[2];

	new_patch = 0x5057454E; // ASCII for "NEWP" signaling new patch
	reset_msg = 0x0U; // reset "NEWP"

	if (flash_read(flash->device, STORAGE_OFFSET, patch_header, sizeof(patch_header))) {
		return -DELTA_PATCH_HEADER_ERROR;
	}

	if (new_patch!=patch_header[0]) {
		return DELTA_OK;
	}

	size = patch_header[1];

	if (flash_write_protection_set(flash->device, false)) {
		return -DELTA_PATCH_HEADER_ERROR;
	}

	if (flash_write(flash->device, STORAGE_OFFSET, &reset_msg, sizeof(reset_msg))) {
		return -DELTA_PATCH_HEADER_ERROR;
	}

	if (flash_write_protection_set(flash->device, true)) {
		return -DELTA_PATCH_HEADER_ERROR;
	}

	return DELTA_OK;
}

const char *delta_error_as_string(int error)
{
	if (error < 28) {
		return detools_error_as_string(error);
	}

	if (error < 0) {
		error *= -1;
	}

	switch (error) {
	case DELTA_SLOT1_OUT_OF_MEMORY:
		return "Slot 1 out of memory.";
	case DELTA_READING_PATCH_ERROR:
		return "Error reading patch.";
	case DELTA_READING_SOURCE_ERROR:
		return "Error reading source image.";
	case DELTA_WRITING_ERROR:
		return "Error writing to slot 1.";
	case DELTA_SEEKING_ERROR:
		return "Seek error.";
	case DELTA_CASTING_ERROR:
		return "Error casting to flash_mem.";
	case DELTA_INVALID_BUF_SIZE:
		return "Read/write buffer less or equal to 0.";
	case DELTA_CLEARING_ERROR:
		return "Could not clear slot 1.";
	case DELTA_NO_FLASH_FOUND:
		return "No flash found.";
	case DELTA_PATCH_HEADER_ERROR:
		return "Error reading patch header.";
	default:
		return "Unknown error.";
	}
}
