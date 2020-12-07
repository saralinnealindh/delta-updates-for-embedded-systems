BOARD := nrf52840dk_nrf52840
BOOT_PATH := bootloader/mcuboot/boot/zephyr

all: bb fb b1 f1

help:
	@echo "Make commands that may be utilized:"	
	@echo "all	build bootloader, flash bootloader, build firmware image and flash firmware image"
	@echo "1	build and flash firmware image"
	@echo "2	build and flash upgrade firmware image to secondary slot"
	@echo "b	build and flash bootloader and erase flash memory"
	@echo "p	create a patch based on 1 and 2 and append "NEWPATCH" to start of it" 
	@echo "		then flash this patch to the storage partition."
	@echo "b1	build the firmware image"		
	@echo "b2	build the upgrade firmware image"
	@echo "bb	build the bootloader"
	@echo "f1	flash the firmware image"
	@echo "f2	flash the upgrade"		
	@echo "fb	erase the flash and flash the bootloader"
	@echo "fp	flash the patch to the storage partition"
	@echo "cp	create a patch based on the firmware image and the upgraded firmware image and"
	@echo "		append NEWPATCH to the beginning of it"
	@echo "c	connect to the device terminal"		

1: b1 f1

2: b2 f2

b: bb fb

p: cp fp

b1: 
	mkdir -p zephyr/build 
	mkdir -p images
	west build -p auto -b $(BOARD) -d zephyr/build app	
	west sign \
		-t imgtool \
		-d zephyr/build/ \
		-H images/signed1.hex \
		--  \
		--version 1.0 \
		--header-size 512 \
		--slot-size 0x67000 \
		--align 4 \
		--key bootloader/mcuboot/root-rsa-2048.pem 
	
b2:
	mkdir -p zephyr/build 
	mkdir -p imagesg
	west build -p auto -b $(BOARD) -d zephyr/build upgrade_app
	west sign \
		-t imgtool \
		-d zephyr/build/ \
		-H images/signed2.hex \
		-- \
		--version 1.0 \
		--header-size 512 \
		--align 4 \
		--slot-size 0x67000 \
		--pad \
		--key bootloader/mcuboot/root-rsa-2048.pem 
	
f1: 
	pyocd flash -e sector -a 0x0c000 -t nrf52840 images/signed1.hex
	
f2: 
	pyocd flash -e sector -a 0x73000 -t nrf52840 images/signed2.hex

fb:
	ninja -C $(BOOT_PATH)/build flash

fp:
	pyocd flash -e sector -a 0xf8000 -t nrf52840 patches/patch.bin

bb:
	mkdir -p $(BOOT_PATH)/build
	cmake -B $(BOOT_PATH)/build -GNinja -DBOARD=BOARD -S $(BOOT_PATH)
	ninja -C $(BOOT_PATH)/build
	
cp:
	mkdir -p patches
	rm -f patches/patch.bin
	detools create_patch --compression heatshrink images/signed1.hex images/signed2.hex patches/patch.bin
	python3 pad_patch.py
	
c:
	JLinkRTTLogger -device NRF52 -if SWD -speed 5000 -rttchannel 0 /dev/stdout
