BOARD := nrf52840dk_nrf52840
PY := python3 

#device flash map
SLOT_SIZE := 0x67000
HEADER_SIZE := 512
SLOT0_OFFSET := 0xc000
SLOT1_OFFSET := 0x73000
PATCH_OFFSET := 0xf8000
MAX_PATCH_SIZE := 0x6000
PATCH_HEADER_SIZE := 0x18 #min 0x10

#relevant directories that the user might have to update
BOOT_DIR := bootloader/mcuboot/boot/zephyr #bootloader image location
BUILD_DIR := zephyr/build #zephyr build directory
KEY_PATH := bootloader/mcuboot/root-rsa-2048.pem #key for signing images

#Names of generated folders and files (can be changed to whatever)
BIN_DIR := binaries
IMG_DIR := $(BIN_DIR)/signed_images
PATCH_DIR := $(BIN_DIR)/patches
DUMP_DIR := $(BIN_DIR)/flash_dumps

SOURCE_PATH := $(IMG_DIR)/source.bin
TARGET_PATH := $(IMG_DIR)/target.bin
PATCH_PATH := $(PATCH_DIR)/patch.bin
SLOT0_PATH := $(DUMP_DIR)/slot0.bin
SLOT1_PATH := $(DUMP_DIR)/slot1.bin

#commands + flags and scripts
PYFLASH := pyocd flash -e sector 
DETOOLS := detools create_patch --compression heatshrink
BUILD_APP := west build -p auto -b $(BOARD) -d $(BUILD_DIR)
SIGN := west sign -t imgtool -d $(BUILD_DIR)
IMGTOOL_SETTINGS := --version 1.0 --header-size $(HEADER_SIZE) \
                    --slot-size $(SLOT_SIZE) --align 4 --key $(KEY_PATH)
PAD_SCRIPT := $(PY) scripts/pad_patch.py
DUMP_SCRIPT := $(PY) scripts/jflashrw.py read
SET_SCRIPT := $(PY) scripts/set_current.py 

all: build-boot flash-boot build flash-image

help:
	@echo "Make commands that may be utilized:"	
	@echo "all                Build + flash bootloader and build"
	@echo "                   + flash firmware."
	@echo "build              Build the firmware image."		
	@echo "build-boot         Build the bootloader."
	@echo "flash-image        Flash the firmware image."
	@echo "flash-boot         Erase the flash and flash the bootloader."
	@echo "flash-patch        Flash the patch to the storage partition."
	@echo "create_patch       1. Create a patch based on the firmware"
	@echo "                     image and the upgraded firmware image."
	@echo "                   2. Append NEWPATCH and patch size to"
	@echo "                     the beginning of the image."
	@echo "connect            Connect to the device terminal."
	@echo "dump-flash         Dump slot 1 and 0 to files."
	@echo "clean              Remove all generated binaries."
	@echo "tools              Install used tools."

build:
	@echo "Building firmware image..."	
	mkdir -p $(BUILD_DIR)
	mkdir -p $(IMG_DIR)
	$(BUILD_APP) app
	$(SIGN) -B $(TARGET_PATH) -- $(IMGTOOL_SETTINGS)

build-boot:
	@echo "Building bootloader..."	
	mkdir -p $(BOOT_DIR)/build
	cmake -B $(BOOT_DIR)/build -GNinja -DBOARD=BOARD -S $(BOOT_DIR)
	ninja -C $(BOOT_DIR)/build
	
flash-image:
	@echo "Flashing latest source image to slot 0..."
	$(SET_SCRIPT) $(TARGET_PATH) $(SOURCE_PATH)
	$(PYFLASH) -a $(SLOT0_OFFSET) -t nrf52840 $(SOURCE_PATH)

flash-boot:
	@echo "Flashing latest bootloader image..."	
	ninja -C $(BOOT_DIR)/build flash

flash-patch:
	@echo "Flashing latest patch to patch partition..."
	$(PYFLASH) -a $(PATCH_OFFSET) -t nrf52840 $(PATCH_PATH)
	$(SET_SCRIPT) $(TARGET_PATH) $(SOURCE_PATH)
	
create-patch:
	@echo "Creating patch..."
	mkdir -p $(PATCH_DIR)
	rm -f $(PATCH_PATH)
	$(DETOOLS) $(SOURCE_PATH) $(TARGET_PATH) $(PATCH_PATH)
	$(PAD_SCRIPT) $(PATCH_PATH) $(MAX_PATCH_SIZE) $(HEADER_SIZE)
	
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
	rm -r -f $(BIN_DIR)

tools:
	@echo "Installing tools..."
	pip3 install --user detools
	pip3 install --user -r bootloader/mcuboot/scripts/requirements.txt
	pip3 install --user pyocd
	pip3 install --user pynrfjprog
	@echo "Done"