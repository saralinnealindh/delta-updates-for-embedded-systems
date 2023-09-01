BOARD := nrf52840dk_nrf52840

#device flash map
SLOT_SIZE := 0x67000
SLOT0_OFFSET := 0xc000
SLOT1_OFFSET := 0x73000

#relevant directories that the user might have to update
BOOT_DIR := ../bootloader/mcuboot/boot/zephyr#bootloader image location
BUILD_DIR := app/build#zephyr build directory
SCRIPT_DIR := ../dfu-bsdiff-zephyr/scripts

#Names of generated folders and files
DUMP_DIR := flash_dumps

SLOT0_PATH := $(DUMP_DIR)/slot0.bin
SLOT1_PATH := $(DUMP_DIR)/slot1.bin

#commands + flags and scripts
DUMP_SCRIPT := python3 $(SCRIPT_DIR)/jflashrw.py read

help:
	@echo "Make commands that may be utilized:"	
	@echo "build-boot         Build the bootloader."
	@echo "flash-boot         Erase the flash and flash the bootloader."
	@echo "connect            Connect to the device terminal."
	@echo "dump-flash         Dump slot 1 and 0 to files."
	@echo "clean              Remove all generated binaries."

build-boot:
	@echo "Building bootloader..."
	mkdir -p $(BOOT_DIR)/build
	cmake -B $(BOOT_DIR)/build -GNinja -DBOARD=$(BOARD) -S $(BOOT_DIR)
	ninja -C $(BOOT_DIR)/build

flash-boot:
	@echo "Flashing latest bootloader image..."	
	ninja -C $(BOOT_DIR)/build flash
	
connect:
	@echo "Connecting to device console.."
	JLinkRTTLogger -device NRF52 -if SWD -speed 5000 -rttchannel 0 /dev/stdout

dump-flash: dump-slot0 dump-slot1

dump-slot0:
	@echo "Dumping slot 0 contents.."
	mkdir -p $(DUMP_DIR)
	rm -f $(SLOT0_PATH)
	touch $(SLOT0_PATH)
	$(DUMP_SCRIPT) --start $(SLOT0_OFFSET) --length $(SLOT_SIZE) --file $(SLOT0_PATH)

dump-slot1:
	@echo "Dumping slot 1 contents.."
	mkdir -p $(DUMP_DIR)
	rm -f $(SLOT1_PATH)
	touch $(SLOT1_PATH)
	$(DUMP_SCRIPT) --start $(SLOT1_OFFSET) --length $(SLOT_SIZE) --file $(SLOT1_PATH)

clean:
	rm -r -f $(BOOT_DIR)/build
	rm -r -f $(BUILD_DIR)
