BOARD := nrf52840dk_nrf52840
BOOT_PATH := bootloader/mcuboot/boot/zephyr

all: build sign flash

build: 
	west build -p auto -b $(BOARD) -d zephyr/build app
	
flash:
	mkdir -p zephyr/build 
	west flash -d zephyr/build --hex-file zephyr/build/zephyr/zephyr.signed.hex
	
sign: 
	west sign -t imgtool -d zephyr/build/ -- --key bootloader/mcuboot/root-rsa-2048.pem

#build-boot:
#	mkdir -p $(BOOT_PATH)/build
#	cmake -B $(BOOT_PATH)/build -GNinja -DBOARD=BOARD -S $(BOOT_PATH)
#	ninja -C $(BOOT_PATH)/build
	
	
	
