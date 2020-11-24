BOARD := nrf52840dk_nrf52840
BOOT_PATH := bootloader/mcuboot/boot/zephyr

all: img1 flash1

upgrade: img2 flash2

boot: build-boot flash-boot

img1: 
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
	
img2:
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
	
flash1: 
	pyocd flash -e sector -a 0x0c000 -t nrf52840 images/signed1.hex
	
flash2: 
	pyocd flash -e sector -a 0x73000 -t nrf52840 images/signed2.hex

flashB:
	ninja -C $(BOOT_PATH)/build flash

flashP:
	pyocd flash -e sector -a 0xf8000 -t nrf52840 patches/patch.bin

build-boot:
	mkdir -p $(BOOT_PATH)/build
	cmake -B $(BOOT_PATH)/build -GNinja -DBOARD=BOARD -S $(BOOT_PATH)
	ninja -C $(BOOT_PATH)/build
	
create-patch:
	mkdir -p patches
	rm -f patches/test.patch
	detools create_patch images/signed1.hex images/signed2.hex patches/patch.bin
	
connect:
	JLinkRTTLogger -device NRF52 -if SWD -speed 5000 -rttchannel 0 /dev/stdout
