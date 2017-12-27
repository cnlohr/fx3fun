/*
 ## Cypress FX3 Firmware Example Source File (cyfxsrammaster.c)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2013-2014,
 ##  All Rights Reserved
 ##  UNPUBLISHED, LICENSED SOFTWARE.
 ##
 ##  CONFIDENTIAL AND PROPRIETARY INFORMATION
 ##  WHICH IS THE PROPERTY OF CYPRESS.
 ##
 ##  Use of this file is governed
 ##  by the license agreement included in the file
 ##
 ##     <install>/license/license.txt
 ##
 ##  where <install> is the Cypress software
 ##  installation root directory path.
 ##
 ## ===========================
*/

/*
   EZ-USB FX3: SRAM Master Firmware Example

   This FX3 firmware example demonstrates how the FX3 can be configured as a master
   for an Asynchronous SRAM device. The example works on the CYUSB3KIT-003 kit from
   Cypress, and makes use of the on-board CY7C1062DV33-10BGXI Asynchronous SRAM.

   As only 8 address pins of the SRAM are connected to FX3, FX3 can only address a
   1 KB space (2 ^ 8 * 32 bits) on the SRAM device. Therefore, this example cannot
   be used to demonstrate the data transfer throughput; and only shows how read and
   write operations to the SRAM can be performed.

   This example makes use of two USB bulk endpoints. A bulk OUT endpoint acts as the
   producer of data from the host. A bulk IN endpoint acts as the consumer of data to
   the host. USB descriptors that describe the device as a Vendor Class USB device are
   used.

   The srammaster.cydsn folder contains the GPIF II Designer project that is used to
   implement the SRAM Master functionality on the FX3 device. The corresponding
   configuration is loaded on the GPIF registers, and a pair of GPIF sockets are used
   to transfer data from/to the SRAM device.

   The U to P DMA channel connects the USB producer (OUT) endpoint to the consumer GPIF
   socket. And the P to U DMA channel connects the producer GPIF socket to the USB
   consumer (IN) endpoint.

   The DMA buffer size for each channel is defined based on the USB speed. 64 for full
   speed, 512 for high speed and 1024 for super speed. CY_FX_SRAM_DMA_BUF_COUNT in the
   header file defines the number of DMA buffers per channel.
 */

#include <cyu3system.h>
#include <cyu3os.h>
#include <cyu3dma.h>
#include <cyu3error.h>
#include <cyu3usb.h>
#include <cyu3uart.h>
#include <cyu3gpif.h>
#include <cyu3pib.h>
#include <pib_regs.h>
#include <cyu3gpio.h>
#include <cyu3utils.h>

#include "fast_gpif2.h"
#include "cyfxgpif2config.h"

static CyU3PThread     AppThread;               /* Application thread object. */

static CyBool_t glIsApplnActive = CyFalse;      /* Whether the loopback application is active or not. */

/* Local buffer used for USB vendor command handling. */
static uint8_t  glEp0Buffer[32] __attribute__ ((aligned (32)));

static uint16_t glLedOnTime  = 0;
static uint16_t glLedOffTime = 0;

/* Application Error Handler: Stub implementation which spins forever. This function can be updated
 * to indicate error by logging or GPIO updates; or to reset the FX3 device and restart the firmware. */
static void
CyFxAppErrorHandler (
        CyU3PReturnStatus_t apiRetStatus)
{
    /* Application failed with the error code apiRetStatus */

    /* Add custom error handling code here. */

    /* Loop Indefinitely */
    for (;;)
    {
        CyU3PThreadSleep (100);
    }
}

/* This function initializes the debug module. The debug prints are routed to the UART
 * and can be seen using a UART console running at 115200 baud rate.
 *
 * Errors in initializing the UART and debug module are not treated as fatal errors.
 */
static void
CyFxSRAMApplnDebugInit (
        void)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUartConfig_t   uartConfig;

    /* Initialize the UART for printing debug messages */
    apiRetStatus = CyU3PUartInit ();
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        return;
    }

    /* Set UART configuration */
    CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
    uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
    uartConfig.stopBit  = CY_U3P_UART_ONE_STOP_BIT;
    uartConfig.parity   = CY_U3P_UART_NO_PARITY;
    uartConfig.txEnable = CyTrue;
    uartConfig.rxEnable = CyFalse;
    uartConfig.flowCtrl = CyFalse;
    uartConfig.isDma    = CyTrue;

    apiRetStatus = CyU3PUartSetConfig (&uartConfig, NULL);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        return;
    }

    /* Set the UART transfer to a really large value. */
    apiRetStatus = CyU3PUartTxSetBlockXfer (0xFFFFFFFF);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        return;
    }

    /* Initialize the debug module. */
    apiRetStatus = CyU3PDebugInit (CY_U3P_LPP_SOCKET_UART_CONS, 8);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        return;
    }

    CyU3PDebugPreamble (CyFalse);
}

/* Initializes the device for the USB <-> SRAM data transfers. */
static void
CyFxSRAMApplnStart (
        void)
{
    CyU3PDmaChannelConfig_t dmaCfg;
    CyU3PEpConfig_t         epCfg;
    CyU3PReturnStatus_t     apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t         usbSpeed = CyU3PUsbGetSpeed ();
    uint16_t                size = 0;

    /* First identify the usb speed. Once that is identified, create a DMA channel and set it up
     * for data transfer. */
    switch (usbSpeed)
    {
        case CY_U3P_FULL_SPEED:
            size         = CY_FX_FULL_SPEED_EP_SIZE;
            glLedOnTime  = 0;
            glLedOffTime = 100;
            break;

        case CY_U3P_HIGH_SPEED:
            size         = CY_FX_HIGH_SPEED_EP_SIZE;
            glLedOnTime  = 100;
            glLedOffTime = 0;
            break;

        case  CY_U3P_SUPER_SPEED:
            size         = CY_FX_SUPER_SPEED_EP_SIZE;
            glLedOnTime  = LED_BLINK_RATE_SS;
            glLedOffTime = LED_BLINK_RATE_SS;
            break;

        default:
            CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Error! Invalid USB speed\r\n");
            CyFxAppErrorHandler (CY_U3P_ERROR_FAILURE);
            break;
    }

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable   = CyTrue;
    epCfg.epType   = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = 1;
    epCfg.pcktSize = size;

    /* Producer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig (CY_FX_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PSetEpConfig failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Consumer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig (CY_FX_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PSetEpConfig failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Create a DMA AUTO channel for USB -> SRAM data transfer. */
    dmaCfg.size           = DMA_OUT_BUF_SIZE;
    dmaCfg.count          = CY_FX_SRAM_DMA_BUF_COUNT_U_2_P;
    dmaCfg.prodSckId      = CY_FX_PRODUCER_USB_SOCKET;
    dmaCfg.consSckId      = CY_FX_CONSUMER_PPORT_SOCKET;
    dmaCfg.dmaMode        = CY_U3P_DMA_MODE_BYTE;
    dmaCfg.notification   = 0;
    dmaCfg.cb             = NULL;
    dmaCfg.prodHeader     = 0;
    dmaCfg.prodFooter     = 0;
    dmaCfg.consHeader     = 0;
    dmaCfg.prodAvailCount = 0;

    apiRetStatus = CyU3PDmaChannelCreate (&glChHandleSRAMUtoP, CY_U3P_DMA_TYPE_AUTO, &dmaCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PDmaChannelCreate failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Create a DMA AUTO channel for SRAM -> USB data transfer. */
    dmaCfg.prodSckId = CY_FX_PRODUCER_PPORT_SOCKET;
    dmaCfg.consSckId = CY_FX_CONSUMER_USB_SOCKET;

    apiRetStatus = CyU3PDmaChannelCreate (&glChHandleSRAMPtoU, CY_U3P_DMA_TYPE_AUTO, &dmaCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PDmaChannelconfig Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Set DMA channel transfer size. */
    apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandleSRAMUtoP, CY_FX_SRAM_DMA_TX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PDmaChannelSetXfer Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandleSRAMPtoU, CY_FX_SRAM_DMA_RX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PDmaChannelSetXfer Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Start the GPIF state machine which will read and write data to/from SRAM whenever requested */
    apiRetStatus = CyU3PGpifSMStart (START, ALPHA_START);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PGpifSMStart Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Update the status flag. */
    glIsApplnActive = CyTrue;
}

/* This functions stops the USB data transfer operation and returns the application to an idle state. */
static void
CyFxSRAMApplnStop (
        void)
{
    CyU3PEpConfig_t     epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Update the flag. */
    glIsApplnActive = CyFalse;

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp (CY_FX_EP_PRODUCER);
    CyU3PUsbFlushEp (CY_FX_EP_CONSUMER);

    /* Destroy the channel */
    CyU3PDmaChannelDestroy (&glChHandleSRAMUtoP);
    CyU3PDmaChannelDestroy (&glChHandleSRAMPtoU);

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    /* Disable the producer endpoint. */
    apiRetStatus = CyU3PSetEpConfig (CY_FX_EP_PRODUCER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PSetEpConfig failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Disable the consumer endpoint. */
    apiRetStatus = CyU3PSetEpConfig (CY_FX_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PSetEpConfig failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

/* Callback to handle the USB setup requests. We only handle vendor commands here, as other commands are
 * handled by the USB driver. The only standard requests handled are interface/endpoint specific requests. */
static CyBool_t
CyFxSRAMApplnUSBSetupCB (
        uint32_t setupdat0,
        uint32_t setupdat1)
{
    uint8_t             bRequest, bReqType;
    uint8_t             bType, bTarget;
    uint16_t            wValue, wIndex, wLength;
    CyBool_t            isHandled = CyFalse;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Decode the fields from the setup request. */
    bReqType = (setupdat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
    bType    = (bReqType & CY_U3P_USB_TYPE_MASK);
    bTarget  = (bReqType & CY_U3P_USB_TARGET_MASK);
    bRequest = ((setupdat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
    wValue   = ((setupdat0 & CY_U3P_USB_VALUE_MASK)   >> CY_U3P_USB_VALUE_POS);
    wIndex   = ((setupdat1 & CY_U3P_USB_INDEX_MASK)   >> CY_U3P_USB_INDEX_POS);
    wLength  = ((setupdat1 & CY_U3P_USB_LENGTH_MASK)  >> CY_U3P_USB_LENGTH_POS);

    if (bType == CY_U3P_USB_STANDARD_RQT)
    {
        /* Handle SET_FEATURE(FUNCTION_SUSPEND) and CLEAR_FEATURE(FUNCTION_SUSPEND)
         * requests here. It should be allowed to pass if the device is in configured
         * state and failed otherwise. */
        if ((bTarget == CY_U3P_USB_TARGET_INTF) && ((bRequest == CY_U3P_USB_SC_SET_FEATURE)
                    || (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)) && (wValue == 0))
        {
            if (glIsApplnActive)
                CyU3PUsbAckSetup ();
            else
                CyU3PUsbStall (0, CyTrue, CyFalse);

            isHandled = CyTrue;
        }

        /* CLEAR_FEATURE request for endpoint is always passed to the setup callback
         * regardless of the enumeration model used. When a clear feature is received,
         * the previous transfer has to be flushed and cleaned up. This is done at the
         * protocol level. Since this is just a loopback operation, there is no higher
         * level protocol. So flush the EP memory and reset the DMA channel associated
         * with it. If there are more than one EP associated with the channel reset both
         * the EPs. The endpoint stall and toggle / sequence number is also expected to be
         * reset. Return CyFalse to make the library clear the stall and reset the endpoint
         * toggle. Or invoke the CyU3PUsbStall (ep, CyFalse, CyTrue) and return CyTrue.
         * Here we are clearing the stall. */
        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)
                && (wValue == CY_U3P_USBX_FS_EP_HALT))
        {
            if (glIsApplnActive)
            {
                if (wIndex == CY_FX_EP_PRODUCER)
                {
                    CyU3PDmaChannelReset (&glChHandleSRAMUtoP);
                    CyU3PUsbFlushEp (CY_FX_EP_PRODUCER);
                    CyU3PUsbResetEp (CY_FX_EP_PRODUCER);
                    CyU3PDmaChannelSetXfer (&glChHandleSRAMUtoP, CY_FX_SRAM_DMA_TX_SIZE);
                }

                if (wIndex == CY_FX_EP_CONSUMER)
                {
                    CyU3PDmaChannelReset (&glChHandleSRAMPtoU);
                    CyU3PUsbFlushEp (CY_FX_EP_CONSUMER);
                    CyU3PUsbResetEp (CY_FX_EP_CONSUMER);
                    CyU3PDmaChannelSetXfer (&glChHandleSRAMPtoU, CY_FX_SRAM_DMA_RX_SIZE);
                }

                /* Clear the stall and mark the request as handled. */
                CyU3PUsbStall (wIndex, CyFalse, CyTrue);
                isHandled = CyTrue;
            }
        }
    }

    if (bType == CY_U3P_USB_VENDOR_RQT)
    {
        switch (bRequest)
        {
            case LED_RATE_CHANGE_COMMAND:
                /* Vendor request to change the LED blink rate. We get the duty cycle for the LED ON time
                 * as a percentage value in byte 0 of the OUT data packet. */
                apiRetStatus = CyU3PUsbGetEP0Data (wLength, glEp0Buffer, NULL);
                if (apiRetStatus == CY_U3P_SUCCESS)
                {
                    if (glEp0Buffer[0] > 100)
                        glLedOnTime = 100;
                    else
                        glLedOnTime = glEp0Buffer[0];
                    glLedOffTime = 100 - glLedOnTime;
                }
                else
                {
                    CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PUsbGetEP0Data Failed, Error code=%d\r\n",
                            apiRetStatus);
                }
                isHandled = CyTrue;
                break;

            case SRAM_READ_COMMAND:
                /* Request to read from SRAM. Switch to the READ part of the GPIF state machine. */
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "SRAM read request\r\n");

                apiRetStatus = CyU3PGpifSMSwitch (256, START1, 256, 0, 0);
                if (apiRetStatus != CY_U3P_SUCCESS)
                {
                    CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PGpifSMSwitch failed, Error code = %d\r\n",
                            apiRetStatus);
                }

                CyU3PUsbAckSetup ();
                isHandled = CyTrue;
                break;

            case DEVICE_RESET_COMMAND:
                /* Ack the request, wait for a second and reset the device. */
                CyU3PUsbAckSetup ();
                CyU3PThreadSleep (1000);
                CyU3PDeviceReset (CyFalse);
                break;

            default:
                isHandled = CyFalse;
                break;
        }
    }

    return isHandled;
}

/* This is the callback function to handle the USB events. We prepare for data transfer on receiving a
 * SETCONF event and return to an idle state when a RESET or DISCONNECT event is received. */
static void
CyFxSRAMApplnUSBEventCB (
    CyU3PUsbEventType_t evtype,
    uint16_t            evdata)
{
    switch (evtype)
    {
        case CY_U3P_USB_EVENT_SETCONF:
            /* Stop the application before re-starting. */
            if (glIsApplnActive)
            {
                CyFxSRAMApplnStop ();
            }

            /* Disable USB Low Power Mode Transitions once the device has been configured. */
            CyU3PUsbLPMDisable ();
            CyFxSRAMApplnStart ();
            break;

        case CY_U3P_USB_EVENT_RESET:
        case CY_U3P_USB_EVENT_DISCONNECT:
            /* Return to idle state. */
            if (glIsApplnActive)
            {
                CyFxSRAMApplnStop ();
            }
            break;

        default:
            break;
    }
}

/* Callback function to handle LPM requests from the USB 3.0 host. This function is invoked by the API
   whenever a state change from U0 -> U1 or U0 -> U2 happens. If we return CyTrue from this function, the
   FX3 device is retained in the low power state. If we return CyFalse, the FX3 device immediately tries
   to trigger an exit back to U0.

   This application does not have any state in which we should not allow U1/U2 transitions; and therefore
   the function always return CyTrue.
 */
static CyBool_t
CyFxApplnLPMRqtCB (
        CyU3PUsbLinkPowerMode link_mode)
{
    return CyTrue;
}

/* P-Port Event Callback. Used to identify error events thrown by the GPIF block.
 * We don't expect errors to happen, and only print a message in case one is reported.
 */
static void
PibEventCallback (
        CyU3PPibIntrType cbType,
        uint16_t         cbArg)
{
    if (cbType == CYU3P_PIB_INTR_ERROR)
    {
        switch (CYU3P_GET_PIB_ERROR_TYPE (cbArg))
        {
            case CYU3P_PIB_ERR_THR0_WR_OVERRUN:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CYU3P_PIB_ERR_THR0_WR_OVERRUN\r\n");
                break;
            case CYU3P_PIB_ERR_THR1_WR_OVERRUN:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CYU3P_PIB_ERR_THR1_WR_OVERRUN\r\n");
                break;
            case CYU3P_PIB_ERR_THR2_WR_OVERRUN:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CYU3P_PIB_ERR_THR2_WR_OVERRUN\r\n");
                break;
            case CYU3P_PIB_ERR_THR3_WR_OVERRUN:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CYU3P_PIB_ERR_THR3_WR_OVERRUN\r\n");
                break;
            case CYU3P_PIB_ERR_THR0_RD_UNDERRUN:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CYU3P_PIB_ERR_THR0_RD_UNDERRUN\r\n");
                break;
            case CYU3P_PIB_ERR_THR1_RD_UNDERRUN:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CYU3P_PIB_ERR_THR1_RD_UNDERRUN\r\n");
                break;
            case CYU3P_PIB_ERR_THR2_RD_UNDERRUN:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CYU3P_PIB_ERR_THR2_RD_UNDERRUN\r\n");
                break;
            case CYU3P_PIB_ERR_THR3_RD_UNDERRUN:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CYU3P_PIB_ERR_THR3_RD_UNDERRUN\r\n");
                break;
            default:
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "PID Error :%d\r\n", CYU3P_GET_PIB_ERROR_TYPE (cbArg));
                break;
        }
    }
}

/* This function initializes the GPIF-II and USB interfaces. */
static void
CyFxSRAMApplnInit (
        void)
{
    CyU3PPibClock_t         pibClock;
    CyU3PGpioClock_t        gpioClock;
    CyU3PGpioSimpleConfig_t gpioConfig;
    CyU3PReturnStatus_t     apiRetStatus = CY_U3P_SUCCESS;

    /* Initialize the p-port block. We are running the PIB Block at 48 MHz. DLL needs to be enabled for Async
     * operation of GPIF. */
    pibClock.clkDiv      = 8;
    pibClock.clkSrc      = CY_U3P_SYS_CLK;
    pibClock.isHalfDiv   = CyFalse;
    pibClock.isDllEnable = CyTrue;
    apiRetStatus = CyU3PPibInit (CyTrue, &pibClock);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "P-port Initialization failed, Error Code=%d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Load the GPIF configuration for SRAM async mode. This loads the state machine for interfacing with SRAM */
    apiRetStatus = CyU3PGpifLoad (&CyFxGpifConfig);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PGpifLoad failed, Error Code=%d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* callback to see if there is any overflow of data on the GPIF II side*/
    CyU3PPibRegisterCallback (PibEventCallback, 0xffff);

    /* Initialize the GPIO module. This is used to update the indicator LED. */
    gpioClock.fastClkDiv = 2;
    gpioClock.slowClkDiv = 0;
    gpioClock.simpleDiv  = CY_U3P_GPIO_SIMPLE_DIV_BY_2;
    gpioClock.clkSrc     = CY_U3P_SYS_CLK;
    gpioClock.halfDiv    = 0;

    apiRetStatus = CyU3PGpioInit (&gpioClock, NULL);
    if (apiRetStatus != 0)
    {
        /* Error Handling */
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PGpioInit failed, error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Override GPIO 54 as this pin is associated with UART CTS signal.
     * The IO cannot be selected as GPIO by CyU3PDeviceConfigureIOMatrix call
     * if the UART block is enabled.
     */
    apiRetStatus = CyU3PDeviceGpioOverride (LED_GPIO, CyTrue);
    if (apiRetStatus != 0)
    {
        /* Error Handling */
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PDeviceGpioOverride failed, error code=%d\r\n",
                apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Configure GPIO 54 as output */
    gpioConfig.outValue    = CyTrue;
    gpioConfig.driveLowEn  = CyTrue;
    gpioConfig.driveHighEn = CyTrue;
    gpioConfig.inputEn     = CyFalse;
    gpioConfig.intrMode    = CY_U3P_GPIO_NO_INTR;

    apiRetStatus = CyU3PGpioSetSimpleConfig (LED_GPIO, &gpioConfig);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        /* Error handling */
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PGpioSetSimpleConfig failed, error code=%d\r\n",
                apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Start the USB functionality. */
    apiRetStatus = CyU3PUsbStart ();
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PUsbStart failed to Start, Error code=%d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register a callback for notification of USB Control Requests. */
    CyU3PUsbRegisterSetupCallback (CyFxSRAMApplnUSBSetupCB, CyTrue);

    /* Setup the callback to handle the USB events. */
    CyU3PUsbRegisterEventCallback (CyFxSRAMApplnUSBEventCB);

    /* Register a callback to handle LPM requests from the USB 3.0 host. */
    CyU3PUsbRegisterLPMRequestCallback (CyFxApplnLPMRqtCB);

    /* Register the SuperSpeed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_SS_DEVICE_DESCR, 0, (uint8_t *)CyFxUSB30DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set Device Desc Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register the High speed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_HS_DEVICE_DESCR, 0, (uint8_t *)CyFxUSB20DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set Device Desc Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register the BOS descriptor */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_SS_BOS_DESCR, 0, (uint8_t *)CyFxUSBBOSDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set BOS Desc Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register the Device qualifier descriptor */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_DEVQUAL_DESCR, 0, (uint8_t *)CyFxUSBDeviceQualDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set DevQual Desc failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register the SuperSpeed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_SS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBSSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set SSConfig Desc failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register the Hi-Speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_HS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBHSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set HSConfig Desc failed, Error Code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register the Full Speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_FS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBFSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set FSConfig Desc failed, Error Code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register String descriptor 0 */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)CyFxUSBStringLangIDDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set String0 Desc failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register String descriptor 1 */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)CyFxUSBManufactureDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set String1 Desc failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Register String descriptor 2 */
    apiRetStatus = CyU3PUsbSetDesc (CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Set String2 Desc failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Enable connection to USB host in SuperSpeed mode. Device will fall back to Hi-Speed or Full Speed as
     * required.
     */
    apiRetStatus = CyU3PConnectState (CyTrue, CyTrue);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "USB Connect failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

/* Entry function for the firmware application thread. */
static void
SRAMAppThread_Entry (
        uint32_t input)
{
    /* Initialize the debug module */
    CyFxSRAMApplnDebugInit ();

    /* Initialize the SRAM application */
    CyFxSRAMApplnInit ();

    /* All transfers are handled automatically. The thread is only responsible for updating the LED. */
    for (;;)
    {
        if (glLedOnTime != 0)
        {
            CyU3PGpioSetValue (LED_GPIO, CyFalse);      /* Turn LED-ON */
            CyU3PThreadSleep (glLedOnTime);             /* ON-time */
        }

        if (glLedOffTime != 0)
        {
            CyU3PGpioSetValue (LED_GPIO, CyTrue);       /* Turn LED-OFF */
            CyU3PThreadSleep (glLedOffTime);            /* OFF-time */
        }
    }
}

/* Application define function which creates the threads. This is a public function as it is called
 * from the FX3 firmware framework, after the RTOS has been started up.
 */
void
CyFxApplicationDefine (
        void)
{
    void     *ptr = NULL;
    uint32_t  retThrdCreate = CY_U3P_ERROR_MEMORY_ERROR;

    /* Allocate the memory for the thread */
    ptr = CyU3PMemAlloc (CY_FX_SRAM_THREAD_STACK);
    if (ptr != NULL)
    {
        /* Create the thread for the application */
        retThrdCreate = CyU3PThreadCreate (&AppThread,  /* SRAM app thread structure */
                "21:FX3_SRAM_Master",                   /* Thread ID and thread name */
                SRAMAppThread_Entry,                    /* FX3-SRAM app thread entry function */
                0,                                      /* No input parameter to thread */
                ptr,                                    /* Pointer to the allocated thread stack */
                CY_FX_SRAM_THREAD_STACK,                /* App Thread stack size */
                CY_FX_SRAM_THREAD_PRIORITY,             /* App Thread priority */
                CY_FX_SRAM_THREAD_PRIORITY,             /* App Thread pre-emption threshold */
                CYU3P_NO_TIME_SLICE,                    /* No time slice for the application thread */
                CYU3P_AUTO_START                        /* Start the thread immediately */
                );
    }

    /* Check the return code */
    if (retThrdCreate != 0)
    {
        /* Thread Creation failed with the error code retThrdCreate. Application cannot continue. */

        /* Add custom recovery or debug actions here */

        /* Loop indefinitely */
        while (1);
    }
}


/*
 * Main function
 */
int
main (
        void)
{
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PSysClockConfig_t clkCfg;
    CyU3PReturnStatus_t   status = CY_U3P_SUCCESS;

    /* setSysClk400 clock configurations */
    clkCfg.setSysClk400  = CyTrue;   /* FX3 device's master clock is set to a frequency > 400 MHz */
    clkCfg.cpuClkDiv     = 2;        /* CPU clock divider */
    clkCfg.dmaClkDiv     = 2;        /* DMA clock divider */
    clkCfg.mmioClkDiv    = 2;        /* MMIO clock divider */
    clkCfg.useStandbyClk = CyFalse;  /* device has no 32KHz clock supplied */
    clkCfg.clkSrc = CY_U3P_SYS_CLK;  /* Clock source for a peripheral block  */

    /* Initialize the device */
    status = CyU3PDeviceInit (&clkCfg);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* Initialize the caches. Enable instruction cache and keep data cache disabled. */
    status = CyU3PDeviceCacheControl (CyTrue, CyFalse, CyFalse);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* Configure the IO matrix for the device. On the FX3 DVK board, the COM port
     * is connected to the IO(53:56). This means that either DQ32 mode should be
     * selected or lppMode should be set to UART_ONLY. */
    io_cfg.useUart          = CyTrue;
    io_cfg.useI2C           = CyFalse;
    io_cfg.useI2S           = CyFalse;
    io_cfg.useSpi           = CyFalse;
    io_cfg.isDQ32Bit        = CyTrue;
    io_cfg.lppMode          = CY_U3P_IO_MATRIX_LPP_DEFAULT;
    io_cfg.gpioSimpleEn[0]  = 0;
    io_cfg.gpioSimpleEn[1]  = 0;
    io_cfg.gpioComplexEn[0] = 0;
    io_cfg.gpioComplexEn[1] = 0;
    io_cfg.s0Mode           = 0;
    io_cfg.s1Mode           = 0;

    status = CyU3PDeviceConfigureIOMatrix (&io_cfg);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* This is a non returnable call for initializing the RTOS kernel */
    CyU3PKernelEntry ();

    /* Dummy return to make the compiler happy */
    return 0;

handle_fatal_error:

    /* Cannot recover from this error. */
    while (1);
}

/* [] */

