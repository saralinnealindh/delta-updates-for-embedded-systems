#include "main.h"

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

	if (ret) 
		printk("Something went wrong :( %X \n",ret);

	/*Main loop*/
	while (1) {
		gpio_pin_set(led, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(SLEEP_TIME_MS);

		if(btn_flag)
		{
			ret = flash_read(flash_pt->device,SECONDARY_OFFSET,buf,length);

			if(ret)
				return;

			patch_size = read_patch_header(flash_pt);

			printk("Size: %X\n",patch_size);
			printk("Flash location: %d\n",flash_pt->device);

			if(patch_size>0) //if patch exists
			{
				printk("Appling patch... \n");
				ret = detools_apply_patch_callbacks(du_flash_from_read,
											du_flash_seek,
											du_flash_patch_read,
											(size_t) patch_size,
											du_flash_write,
											flash_pt);
				if(ret<=0)
					printk("Error: %d \n",ret);
			}

			btn_flag = false;
		}
	}
}

static int du_flash_write(void *arg_p,
					const uint8_t *buf_p,
					size_t size)
{
	struct flash_mem *flash; 
    flash = (struct flash_mem *)arg_p;

	if(!flash)
		return 0x18;//couldnt convert to flash_mem

	/*if (flash->to_current%4) 
		return 0x12; //if word not aligned*/

	if (flash_write_protection_set(flash->device, false)) 
		return 0x13; //write protection could not be removed

	if (flash_write(flash->device, flash->to_current,buf_p, size)) 
		return 0x14; //writing could not be done

	if (flash_write_protection_set(flash->device, true))
		return 0x15; //writing protection could not be set again

	flash->to_current += (off_t) size;

	if (flash->to_current>=flash->to_end)
		return 0x1; //out of memory

	return 0;
}

static int du_flash_from_read(void *arg_p,
					uint8_t *buf_p,
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
					uint8_t *buf_p,
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

static int read_patch_header(struct flash_mem *flash)
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

	//BELOW IS CODE USED TO MARK PATCH AS APPLIED. DURING TESING NOT USED. 

	/*if (flash_write_protection_set(flash->device, false)) 
		return -0x3; //write protection could not be removed

	if (flash_write(flash->device, STORAGE_OFFSET,new_word, word_len)) 
		return -0x4; //writing could not be done

	if (flash_write_protection_set(flash->device, true))
		return -0x5; //writing protection could not be set again*/

	return size;
}

static int init_flash_mem(struct flash_mem *flash)
{
	if(!flash)
		return -1;

	flash->from_current = PRIMARY_OFFSET;
	flash->from_end = flash->from_current + PRIMARY_SIZE;

	flash->to_current = SECONDARY_OFFSET;
	flash->to_end = flash->to_current + SECONDARY_SIZE;

	flash->patch_current = STORAGE_OFFSET + HEADER_SIZE;
	flash->patch_end = flash->patch_current + STORAGE_SIZE;

	return 0;
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

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
	btn_flag = true;
}