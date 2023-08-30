# About
This is an example program showcasing an implementation of [DETools](https://github.com/eerimoq/detools) for [Zephyr](https://www.zephyrproject.org/). It allows for incremental firmware updates, or [delta updates](https://en.wikipedia.org/wiki/Delta_update), as an alternative to the standard procedure of downloading new firmware in its entirety. 

The program itself is a modification of the Zephyr sample program "Blinky" (which flashes LED 1 on a board) with the added functionality that when button 1 is pressed the program checks for a new patch and, if such a patch exists, performs a firmware upgrade. A developer may easily modify the program code to make the application flash LED 2 instead, create a patch, download it to the board, push button 1, and confirm whether the upgrade was successful by checking which LED is flashing.

### Key features 
The program was created for my [bachelor´s thesis](https://hdl.handle.net/20.500.12380/302598), which one may look through for implementation details, descriptions of the algorithms used, methodology, and suggestions for further research, among other things. For a brief overview one may refer to the list below:

* The program is currently hardware specific and assumes the Nordic [nRF52840 DK](https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk) SoC is used. However, it will likely very easily be ported to other [Zephyr supported boards](https://docs.zephyrproject.org/latest/boards/index.html).
* Zephyr v3.3 is currently used on the master branch, however there is also a branch for v2.4 (which was originally used).
* Downloading firmware to the device is currently only supported using the USB interface.
* The delta encoding algorithm used is the DETools implementation of [BSDiff](http://www.daemonology.net/bsdiff/) using [heat-shrink](https://github.com/atomicobject/heatshrink) for compression.
* The program utilizes the Device Firmware Upgrade features facilitated by the [MCUBoot](https://www.mcuboot.com/) bootloader, and is therefore dependent on its usage (MCUBoot is automatically included if one follows the environment set up steps below). Most notably it takes advantage of the [flash map layout](https://github.com/mcu-tools/mcuboot/blob/main/docs/readme-zephyr.md).
* The patching process makes use of three partitions: the primary partition, the secondary partition, and the patch partition. The current firmware runs on the primary partition, the new firmware is created on the secondary partition, and the patch is downloaded to the patch partition. Naturally, this means that the size of the patch partition puts an upper limit to the size of the patch. When the new firmware has been created, the device requests a swap of the primary and the secondary partition and reboots, as in the [normal upgrade scanario](https://www.mcuboot.com/documentation/design/#high-level-operation).
* Creation of patches, downloading firmware to the device, building, and a few other things may easily be done using the make commands provided in the makefile. A full list of these commands can be acquired using `make help`.
* Limited testing has resulted in patch sizes of 1.6 to 6.4 percent of target image size depending on the types of changes made. However, more extended testing has to be performed in order to make any generalized claims.

### Purpose 
Many resource-constrained IoT units are used in a manner which makes them inaccessible via cable and at the same time unable to receive large amounts of data using radio transmissions. Upgrading firmware is hence difficult or in some cases impossible. Integrating support for delta updates in the unit is a potential solution to this problem, as it significantly reduces the payload during an upgrade scenario. However, as of now there is no open-source solution for delta upgrades in embedded systems. The purpose of the application is to show that such a solution is achievable and that it causes a significant payload reduction. 

# Environment Setup
Follow Zephyr's [Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) up to step 3.2 "Get the Zephyr source code". Here one should run the commands below instead of the ones in the guide:

    $ git clone https://gitlab.endian.se/thesis-projects/delta-updates-for-embedded-systems.git
    $ cd delta-updates-for-embedded-systems
    $ west init -l
    $ west update
    $ west zephyr-export
    $ pip3 install --user -r ./zephyr/scripts/requirements.txt

Then complete the remaining steps under section 4.

Additionally, one will need some external development tools. Return to the project folder and run `make tools` to install the needed python packages. Download and install [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download#infotabs) and [J-Link Software](https://www.segger.com/downloads/jlink/) to enable some utilities for flashing and debugging the device.

# Example Usage
This small guide features some examples of how to use the program. A good place to start might be to perform them all sequentially. 

### Build and flash the bootloader
If the device currently does not have MCUBoot installed the first step is to build it and download it to the device, which one does with: 

    $ make build-boot
    $ make flash-boot

### Build and flash the Zephyr program
The next step might be to build and flash the firmware. 

    $ make build
    $ make flash-image

When executing the second command one will get a prompt asking if this version should be set as the currently running one, to which you may respond `y`. This functionality exists as some rudimentary way of keeping track of versions when creating a patches. 

### Modify the program to flash another LED
In order to test the patching algorithm a new version of the program is needed. This is easiest done by modifying which LED light is flashing. This can be done by opening `delta-updates-for-embedded-systems/app/src/main.c` in ones editor of choice and replacing `#define LED0_NODE DT_ALIAS(led0)` with `#define LED0_NODE DT_ALIAS(led1)` (or some other LED). The "new" application can then be built with:

    $ make build

### Create and flash the patch
Creating a patch requires step 2 and 3 to at some point have been completed, as a "currently running" and a "latest created" version of the software has to have been set. The patch created will contain instructions for how to transform the former of these into the later. 

The commands for creating the patch and downloading it the patch partition are: 

    $ make create-patch
    $ make flash-patch

After executing the second command one will get a prompt asking if this version should be set as the currently running one. To this one might want to respond `y` if the upgrade was successful or `n` if it was not.

### Upgrade the firmware
When the patch is downloaded to the patch partition and the program is flashing LED 1 it is time to start the patching process, which one does by clicking button 1. The LED should stop blinking for a few seconds while its creating the new firmware and reboots, and then start up again doing whatever one modified the new program to do. 

# Notable changes


| Date | Comment | Commit |
| ------ | ------ | ------ |
| **20210927**| Patch header is now shorter (8 bytes compared to 24 bytes). Patch size is no longer a string. "NEWPATCH" has been shortened to "NEWP". | [cfa78347](https://gitlab.endian.se/thesis-projects/delta-updates-for-embedded-systems/-/commit/cfa78347cefd7b46330c46a17ffad75ccb58abee) |
| **20230525**| Lifted to Zephyr v3.3 | [15a8689](https://github.com/saralinnealindh/delta-updates-for-embedded-systems/commit/15a86891df7f148e71a3ea23763e1cf1c0e8f3bb) |
