#include "delta.h"

/* 
*  IMAGE/FLASH MANAGEMENT
*/

static int delta_flash_write(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p;

	if(!flash)
		return -DELTA_CASTING_ERROR;

	if (flash_write_protection_set(flash->device, false)) 
		return -DELTA_WRITING_ERROR; 

	if (flash_write(flash->device, flash->to_current,buf_p, size)) 
		return -DELTA_WRITING_ERROR;

	if (flash_write_protection_set(flash->device, true))
		return -DELTA_WRITING_ERROR; 

	flash->to_current += (off_t) size;

	if (flash->to_current>=flash->to_end)
		return -DELTA_SLOT1_OUT_OF_MEMORY; 

	return DELTA_OK;
}

static int delta_flash_from_read(void *arg_p,
					uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p;

	if(!flash)
		return -DELTA_CASTING_ERROR;

	if (size <= 0) 
		return -DELTA_INVALID_BUF_SIZE; 

	if(flash_read(flash->device,flash->from_current,buf_p,size))
		return -DELTA_READING_SOURCE_ERROR; 

	flash->from_current += (off_t) size;

	if (flash->from_current>=flash->from_end)
		return -DELTA_READING_SOURCE_ERROR; 

	return DELTA_OK;
}

static int delta_flash_patch_read(void *arg_p,
					uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p;

	if(!flash)
		return -DELTA_CASTING_ERROR; 

	if (size <= 0)
		return -DELTA_INVALID_BUF_SIZE;

	if (flash_read(flash->device,flash->patch_current,buf_p,size))
		return -DELTA_READING_PATCH_ERROR;
	
	flash->patch_current += (off_t) size;

	if (flash->patch_current>=flash->patch_end)
		return -DELTA_READING_PATCH_ERROR; 
	
	return DELTA_OK;
}

static int delta_flash_seek(void *arg_p,int offset)
{
	struct flash_mem *flash;
    flash = (struct flash_mem *)arg_p;

	if(!flash)
		return -DELTA_CASTING_ERROR; 

	flash->from_current += offset;

	if (flash->from_current>=flash->from_end)
		return -DELTA_SEEKING_ERROR;

	return DELTA_OK;
}

/* 
*  INIT
*/

static int delta_init_flash_mem(struct flash_mem *flash)
{
	if(!flash)
		return -DELTA_NO_FLASH_FOUND;

	flash->from_current = PRIMARY_OFFSET;
	flash->from_end = flash->from_current + PRIMARY_SIZE;

	flash->to_current = SECONDARY_OFFSET;
	flash->to_end = flash->to_current + SECONDARY_SIZE;

	flash->patch_current = STORAGE_OFFSET + HEADER_SIZE;
	flash->patch_end = flash->patch_current + STORAGE_SIZE;

	return DELTA_OK;
}

static int delta_clear_slot1(const struct device *flash) 
{
	if (flash_write_protection_set(flash, false)) 
		return -DELTA_CLEARING_ERROR;

	if (flash_erase(flash,SECONDARY_OFFSET,SECONDARY_SIZE)) 
		return -DELTA_CLEARING_ERROR; 

	if (flash_write_protection_set(flash, true))
		return -DELTA_CLEARING_ERROR; 

	return DELTA_OK; 
}

static int delta_init(struct flash_mem *flash)
{
	int ret;

	ret = delta_init_flash_mem(flash);

    if(ret)
		return ret;

	ret = delta_clear_slot1(flash->device);

    if(ret)
        return ret;

    return DELTA_OK;
}

/*
*  PUBLIC FUNCTIONS 
*/

int delta_check_and_apply(struct flash_mem *flash)
{
    int patch_size,ret;

    patch_size = delta_read_patch_header(flash);

    if(patch_size<0)
        return patch_size;

    else if(patch_size>0)
    {
		ret = delta_init(flash);

		if(ret)
			return ret;

        ret = detools_apply_patch_callbacks(delta_flash_from_read,
											delta_flash_seek,
											delta_flash_patch_read,
											(size_t) patch_size,
											delta_flash_write,
											flash);
		if(ret<=0)
			return ret;

        if(boot_request_upgrade(BOOT_UPGRADE_PERMANENT))
			return -1;
				
		sys_reboot(SYS_REBOOT_COLD);
    }
    
    return 0;
}

int delta_read_patch_header(struct flash_mem *flash)
{
	int header_len, word_len, size;
	word_len = 0x8;
	header_len = HEADER_SIZE;
	unsigned char *buf_p,*buf_end,buf[header_len];
	unsigned char word_buf[] = {'N','E','W','P','A','T','C','H'};
	unsigned char new_word[] = {'F','F','F','F','F','F','F','F'};

	if(flash_read(flash->device,STORAGE_OFFSET,buf,header_len))
		return -0x7; //could not read flash

	if(strncmp(buf,word_buf,word_len))
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

const char *delta_error_as_string(int error)
{
	if (error<28)
		return detools_error_as_string(error);

	if (error < 0) {
        error *= -1;
    }

	switch(error)
	{
    case DELTA_SLOT1_OUT_OF_MEMORY:
		return "Slot 1 out of memory.";
	case DELTA_READING_PATCH_ERROR:
		return "Error reading patch.";
	case DELTA_READING_SOURCE_ERROR:
		return "Error reading source image.";
	case DELTA_WRITING_ERROR:
		return "Error writing to slot 1";
	case DELTA_SEEKING_ERROR:
		return "Seek error";
	case DELTA_CASTING_ERROR:
		return "Error casting to flash_mem";
	case DELTA_INVALID_BUF_SIZE:
		return "Read/write buffer less or equal to 0";
	case DELTA_CLEARING_ERROR:
		return "Could not clear slot 1";
	case DELTA_NO_FLASH_FOUND:
		return "No flash found";
	default:
		return "Unknown error";
	}
}