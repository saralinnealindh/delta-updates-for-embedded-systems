# About

This is a example program showcasing an implementation of [DETools](https://github.com/eerimoq/detools) for [Zephyr](https://www.zephyrproject.org/) which enables firmware patching.

The program was created for my [bachelor´s thesis](https://hdl.handle.net/20.500.12380/302598), which one may look through for implementation details, descriptions of the algorithms used, methodology, and suggestions for further research, among other things. Additionally, a brief summary of some key features will be outlined below.

* The delta encoding algorithm used is the DETools implementation of [BSDiff](http://www.daemonology.net/bsdiff/) using [heat-shrink](https://github.com/atomicobject/heatshrink) for compression.  
* The implementation is currently hardware specific and assumes the [nRF52840 DK](https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk) board is used. 

### Purpose 
Many resource-constrained IoT units are used in a manner which makes them inaccessible via cable and at the same time unable to receive large amounts of data using radio transmissions. Upgrading firmware is hence difficult or in some cases impossible. Integrating support for delta updates in the unit is a potential solution to this problem, as it significantly reduces the payload during an upgrade scenario. However, as of now there is no open-source solution for delta upgrades in embedded systems. The purpose of the application is to such a solution is achievable and that it causes a significant payload reduction. 

# Environment Setup
Follow Zephyr's [Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) up to step 3.2 "Get the Zephyr source code". Here one should run the commands below instead of the ones in the guide:

    $ git clone https://gitlab.endian.se/thesis-projects/delta-updates-for-embedded-systems.git
    $ cd delta-updates-for-embedded-systems
    $ west init -l app/
    $ west update

Then complete the remaining steps under section 3 and 4. Finally, run `make tools`.

# Example Usage
