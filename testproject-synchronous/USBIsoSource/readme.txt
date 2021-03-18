
                        CYPRESS SEMICONDUCTOR CORPORATION
                                    FX3 SDK

USB ISO SOURCE EXAMPLE
----------------------

  This example illustrates the use of the FX3 firmware APIs to implement
  a variable data rate ISO IN endpoint.

  The device enumerates as a vendor specific USB device with an ISO IN endpoint
  (3-IN).

  The data transfer is achieved with the help of a a DMA MANUAL_OUT channel.
  A constant data pattern is continuously loaded into the DMA MANUAL_OUT channel
  and sent to the host. Multiple alternate settings with different transfer
  rates are supported.

  Files:

    * cyfx_gcc_startup.S   : Start-up code for the ARM-9 core on the FX3 device.
      This assembly source file follows the syntax for the GNU assembler.

    * cyfxisosrc.h         : Constant definitions for the iso source
      application. The properties of the endpoint can be selected through
      definitions in this file.

    * cyfxisodscr.c        : C source file containing the USB descriptors that
      are used by this firmware example.

    * cyfxtx.c             : ThreadX RTOS wrappers and utility functions required
      by the FX3 API library.

    * cyfxisosrc.c         : Main C source file that implements the iso source
      example.

    * makefile             : GNU make compliant build script for compiling this
      example.

[]

