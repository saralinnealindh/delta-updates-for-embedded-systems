
# About
This is a short example program showcasing an implementation of [DETools](https://github.com/eerimoq/detools) for [Zephyr](https://www.zephyrproject.org/) which enables firmware patching.

The program was created for my [bachelor´s thesis](https://hdl.handle.net/20.500.12380/302598), which one may look through for implementation details, desriptions of the algorithms used, methodology, and suggestions for further research, among other things. Additionally, a brief summary of some key features will be outlined below.

* The delta encoding algorithm used is the DETools implementation of [BSDiff](http://www.daemonology.net/bsdiff/) using [heat-shrink](https://github.com/atomicobject/heatshrink) for compression.  
* The implementation is currently hardware specific and assumes the [nRF52840 DK](https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk) board is used. 

# Environment Setup
Follow Zephyr's [Getting Started Guide](https://docs.zephyrproject.org/latest/getting_started/index.html) up to step 3.2 "Get the Zephyr source code". Here one should run the commands below instead of the ones in the guide:

    $ git clone https://gitlab.endian.se/thesis-projects/delta-updates-for-embedded-systems.git
    $ cd delta-updates-for-embedded-systems
    $ west init -l app/
    $ west update

Then complete the remaining steps under section 3 and 4. Finally, run `make tools`.

# Example Usage
