/*
 ## Cypress USB 3.0 Platform source file (cyfxisosrc.c)
 ## ===========================
 ##
 ##  Copyright Cypress Semiconductor Corporation, 2010-2011,
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

/* This file illustrates the ISO source Application example using the DMA MANUAL_OUT mode */

/*
   This example illustrates USB endpoint data source mechanism, as well as the handling
   of USB SET_INTERFACE commands.

   A single ISO IN endpoint is supported with different data rates on different alternate
   interfaces. The endpoint is configured for the appropriate data rate when the SET_INTERFACE
   command is received and processed.

   A constant patern data is loaded onto the OUT channel DMA buffer whenever the buffer is available.
   CPU issues commit of the DMA data transfer to the consumer endpoint which then gets transferred 
   to the host. This leads to a constant source mechanism.
 */

#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyfxisosrc.h"
#include "cyu3usb.h"
#include "cyu3uart.h"
#include "cyu3i2c.h"
#include <cyu3gpif.h> //XXX CNL
#include <cyu3pib.h>
#include <pib_regs.h>
#include <cyu3gpio.h>

#include "fast_gpif2.cydsn/cyfxgpif2config.h"
//#include "fast_gpif2.h"
//#include "cyfxgpif2config.h"

CyU3PThread     isoSrcAppThread;	/* ISO loop application thread structure */
CyU3PDmaChannel glChHandleIsoSrc;       /* DMA MANUAL_OUT channel handle */


uint32_t DataOverrunErrors = 0;
uint32_t dmaevent;
uint32_t KeepDataAlive = 0;	//XXX Tricky: The chip seems to hose itself if you let it run with nothing consuming the data.  Do this to make sure we only run for a little while before getting some messages back from the host on the control ports.

CyBool_t glIsApplnActive = CyFalse;     /* Whether the loopback application is active or not. */
uint32_t glDMARxCount = 0;              /* Counter to track the number of buffers received. */
uint32_t glDMATxCount = 0;              /* Counter to track the number of buffers transmitted. */

uint8_t  glAltIntf    = 0;              /* Currently selected Alternate Interface number. */
uint32_t glCtrlDat0   = 0;              /* bmRequestType + bRequest + wValue from control request. */
uint32_t glCtrlDat1   = 0;              /* wIndex + wLength from control request. */
uint8_t  *glEp0Buffer = 0;              /* Buffer used to handle vendor specific control requests. */

CyU3PEvent glAppEvent;                  /* Event group used to defer handling of vendor specific control requests. */
#define CYFX_ISOAPP_CTRL_TASK   1       /* Deferred event flag indicating pending control request. */
#define CYFX_ISOAPP_DOUT_RQT    0xD1    /* Example request with OUT data phase. */
#define CYFX_ISOAPP_DIN_RQT     0xD2    /* Example request with IN data phase. */
#define CYFX_ISOAPP_NODATA_RQT  0xD3    /* Example request with NO data phase. */
#define CYFX_ISOAPP_MAX_EP0LEN  64      /* Max. data length supported for EP0 requests. */

/* Application Error Handler */
void
CyFxAppErrorHandler (
        CyU3PReturnStatus_t apiRetStatus    /* API return status */
        )
{
    /* Application failed with the error code apiRetStatus */

    /* Add custom debug or recovery actions here */

    /* Loop Indefinitely */
    for (;;)
    {
        /* Thread sleep : 100 ms */
        CyU3PThreadSleep (100);
    }
}

/* This function initializes the debug module. The debug prints
 * are routed to the UART and can be seen using a UART console
 * running at 115200 baud rate. */
void
CyFxIsoSrcApplnDebugInit (void)
{
    CyU3PUartConfig_t   uartConfig;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PI2cConfig_t    i2cConfig;

    /* Initialize the UART for printing debug messages */
    apiRetStatus = CyU3PUartInit();
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        /* Error handling */
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set UART configuration */
    CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
    uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
    uartConfig.stopBit = CY_U3P_UART_ONE_STOP_BIT;
    uartConfig.parity = CY_U3P_UART_NO_PARITY;
    uartConfig.txEnable = CyTrue;
    uartConfig.rxEnable = CyFalse;
    uartConfig.flowCtrl = CyFalse;
    uartConfig.isDma = CyTrue;

    apiRetStatus = CyU3PUartSetConfig (&uartConfig, NULL);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set the UART transfer to a really large value. */
    apiRetStatus = CyU3PUartTxSetBlockXfer (0xFFFFFFFF);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Initialize the debug module. */
    apiRetStatus = CyU3PDebugInit (CY_U3P_LPP_SOCKET_UART_CONS, 8);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Initialize and configure the I2C block. */
    apiRetStatus = CyU3PI2cInit ();
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler (apiRetStatus);
    }

    CyU3PMemSet ((uint8_t *)&i2cConfig, 0, sizeof(i2cConfig));
    i2cConfig.bitRate    = 100000;
    i2cConfig.busTimeout = 0xFFFFFFFF;
    i2cConfig.dmaTimeout = 0xFFFF;
    i2cConfig.isDma      = CyFalse;
    apiRetStatus = CyU3PI2cSetConfig (&i2cConfig, NULL);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler (apiRetStatus);
    }
}

void DMACallback (
        CyU3PDmaChannel   *chHandle, /* Handle to the DMA channel. */
        CyU3PDmaCbType_t  type,      /* Callback type.             */
        CyU3PDmaCBInput_t *input)    /* Callback status.           */
{
	dmaevent++;

    CyU3PDebugPrint (4, "DMA EVENT\r\n", type);

}

/* This function starts the ISO loop application. This is called
 * when a SET_CONF event is received from the USB host. The endpoints
 * are configured and the DMA pipe is setup in this function. */
void
CyFxIsoSrcApplnStart (
        void)
{
    uint16_t size = 0, index = 0;
    CyU3PEpConfig_t epCfg;
    //CyU3PDmaBuffer_t buf_p;
    CyU3PDmaChannelConfig_t dmaCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();
    uint8_t isoPkts = 1;

    CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Starting initialization speed = %d\r\n", usbSpeed);

    /* First identify the usb speed. Once that is identified,
     * create a DMA channel and start the transfer on this. */

    /* Based on the Bus Speed configure the endpoint packet size */
    switch (usbSpeed)
    {
        case CY_U3P_FULL_SPEED:
            size    = 64;
            isoPkts = 1;                /* One packet per frame. */
            break;

        case CY_U3P_HIGH_SPEED:
            size    = 1024;
            isoPkts = CY_FX_ISO_PKTS;   /* CY_FX_ISO_PKTS packets per microframe. */
            break;

        case  CY_U3P_SUPER_SPEED:
            size = 1024;
            if (glAltIntf == 1)
                isoPkts = 1;                    /* One burst per microframe. */
            else
                isoPkts = CY_FX_ISO_PKTS;       /* CY_FX_ISO_PKTS bursts per microframe. */
            break;

        default:
            CyU3PDebugPrint (4, "Error! Invalid USB speed.\n");
            CyFxAppErrorHandler (CY_U3P_ERROR_FAILURE);
            break;
    }

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable   = CyTrue;
    epCfg.epType   = CY_U3P_USB_EP_ISO;
    epCfg.burstLen = (usbSpeed == CY_U3P_SUPER_SPEED) ? (CY_FX_ISO_BURST) : 1;
    epCfg.streams  = 0;
    epCfg.pcktSize = size;
    epCfg.isoPkts  = isoPkts;

    /* Consumer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

    /* Only the EP needs to be configured with the correct parameters. The DMA channel can always be prepared
       to allow the greatest bandwidth possible.
     */
    dmaCfg.size = ((size + 0x0F) & ~0x0F);
    if (usbSpeed != CY_U3P_FULL_SPEED)
    {
        dmaCfg.size *= CY_FX_ISO_PKTS;
    }

    /* Multiply the buffer size with the burst value for performance improvement. */
    //dmaCfg.size          *= CY_FX_ISO_BURST;//XXX CNL This should be DMA_OUT_BUF_SIZE shouldn't it?
    //dmaCfg.size           = DMA_IN_BUF_SIZE;
    dmaCfg.size           = 32768;//XXX CNL This should be DMA_OUT_BUF_SIZE shouldn't it?
    dmaCfg.count          = CY_FX_ISOSRC_DMA_BUF_COUNT;
    dmaCfg.prodSckId      = CY_FX_PRODUCER_PPORT_SOCKET; // Was CY_U3P_CPU_SOCKET_PROD; XXX CNLohr
    dmaCfg.consSckId      = CY_FX_EP_CONSUMER_SOCKET;
    dmaCfg.dmaMode        = CY_U3P_DMA_MODE_BYTE; //Doesn't fix it if moving to CY_U3P_DMA_MODE_BUFFER :(
    dmaCfg.notification   = 0xffffff; //CY_U3P_DMA_CB_CONS_EVENT;
    dmaCfg.cb             = DMACallback ; //CyFxIsoSrcDmaCallback; XXX CNL
    dmaCfg.prodHeader     = 0;
    dmaCfg.prodFooter     = 0;
    dmaCfg.consHeader     = 0;
    dmaCfg.prodAvailCount = 0;

    CyU3PDebugPrint (4, "DMA Config Size = %d\r\n", dmaCfg.size);

    //apiRetStatus = CyU3PDmaChannelCreate (&glChHandleIsoSrc, CY_U3P_DMA_TYPE_MANUAL_OUT, &dmaCfg); XXX CNL
    apiRetStatus = CyU3PDmaChannelCreate (&glChHandleIsoSrc, CY_U3P_DMA_TYPE_AUTO, &dmaCfg);

    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDmaChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandleIsoSrc, CY_FX_ISOSRC_DMA_TX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

#if 0
    /* I don't think we want to do this in our case. */

    /* Now preload all buffers in the MANUAL_OUT pipe with the required data. */
    for (index = 0; index < CY_FX_ISOSRC_DMA_BUF_COUNT; index++)
    {
        apiRetStatus = CyU3PDmaChannelGetBuffer (&glChHandleIsoSrc, &buf_p, CYU3P_NO_WAIT);
        if (apiRetStatus != CY_U3P_SUCCESS)
        {
            CyU3PDebugPrint (4, "CyU3PDmaChannelGetBuffer failed, Error code = %d\n", apiRetStatus);
            CyFxAppErrorHandler(apiRetStatus);
        }
        CyU3PMemSet (buf_p.buffer, CY_FX_ISOSRC_PATTERN, buf_p.size);
        apiRetStatus = CyU3PDmaChannelCommitBuffer (&glChHandleIsoSrc, buf_p.size, 0);
        if (apiRetStatus != CY_U3P_SUCCESS)
        {
            CyU3PDebugPrint (4, "CyU3PDmaChannelCommitBuffer failed, Error code = %d\n", apiRetStatus);
            CyFxAppErrorHandler(apiRetStatus);
        }
    }
#endif

    //////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////Added by Charles

    /* Create a DMA AUTO channel for SRAM -> USB data transfer. */

    /* Set DMA channel transfer size. */

/*
    apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandleSRAMPtoU, CY_FX_SRAM_DMA_RX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PDmaChannelSetXfer Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
*/

    /* Start the GPIF state machine */
    //apiRetStatus = CyU3PGpifSMStart (STARTNULL, ALPHA_STARTNULL);


#if 0
    apiRetStatus = CyU3PGpifSMStart (STARTRX, ALPHA_STARTRX);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PGpifSMStart Failed, Error code = %d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
#endif
    //////////////////////////////////////////////////////////////////////////////////////////////////////////




    /* Update the flag so that the application thread is notified of this. */
    glIsApplnActive = CyTrue;
}

/* This function stops the ISO loop application. This shall be called whenever
 * a RESET or DISCONNECT event is received from the USB host. The endpoints are
 * disabled and the DMA pipe is destroyed by this function. */
void
CyFxIsoSrcApplnStop (
        void)
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "De-initializing...\r\n");

    //Stop the GPIF bus.
    CyU3PGpifDisable( 0 );


    /* Update the flag so that the application thread is notified of this. */
    glIsApplnActive = CyFalse;

    /* Destroy the channels */
    CyU3PDmaChannelDestroy (&glChHandleIsoSrc);

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp (CY_FX_EP_CONSUMER);

    /* Disable endpoints. */
    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    /* Consumer endpoint configuration. */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }
}

/* Callback to handle the USB setup requests. */
CyBool_t
CyFxIsoSrcApplnUSBSetupCB (
        uint32_t setupdat0, /* SETUP Data 0 */
        uint32_t setupdat1  /* SETUP Data 1 */
    )
{
    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and interface/endpoint control requests are received by this function.
     */

    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue;
    CyBool_t isHandled = CyFalse;

    /* Decode the fields from the setup request. */
    bReqType = (setupdat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
    bType    = (bReqType & CY_U3P_USB_TYPE_MASK);
    bTarget  = (bReqType & CY_U3P_USB_TARGET_MASK);
    bRequest = ((setupdat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
    wValue   = ((setupdat0 & CY_U3P_USB_VALUE_MASK)   >> CY_U3P_USB_VALUE_POS);

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
    }

    /* Defer handling all vendor specific control requests to the application thread. */
    if (bType == CY_U3P_USB_VENDOR_RQT)
    {
        glCtrlDat0 = setupdat0;
        glCtrlDat1 = setupdat1;
        isHandled  = CyTrue;            /* Tell the driver that this request has been handled, so that
                                           the driver does not take any default action. */

        //XXX NOTE: Could handle special requests in here if really low latency is needed.
        if( bRequest == 0xaa )
        {
        	uint32_t sendback[10];
        	sendback[0] = 0xaabbccdd;
        	sendback[1] = DataOverrunErrors;
        	sendback[2] = dmaevent;
            CyU3PUsbSendEP0Data (12, sendback);  //Sends back "hello"
            if( KeepDataAlive == 0 )
            {
            	CyU3PGpifSMStart (STARTRX, ALPHA_STARTRX);
            }
            KeepDataAlive = 12000*2; //~1.9 seconds.
        }
        else
        {
        	/* Send an event to the thread, asking for this control request to be handled. */
        	CyU3PEventSet (&glAppEvent, CYFX_ISOAPP_CTRL_TASK, CYU3P_EVENT_OR);
        }
    }

    return isHandled;
}

/* This is the callback function to handle the USB events. */
void
CyFxIsoSrcApplnUSBEventCB (
    CyU3PUsbEventType_t evtype, /* Event type */
    uint16_t            evdata  /* Event data */
    )
{
    switch (evtype)
    {
        /* In this ISO application, channels are created and destroyed when an interface is selected.
           If Alt Setting 0 is selected for interface 0, channels can be freed up.
           If any other Alt Setting is selected, the channels are created afresh.
         */
        case CY_U3P_USB_EVENT_SETINTF:
            /* Make sure to remove any previous configuration, before starting afresh. */
            if (glIsApplnActive)
            {
                CyFxIsoSrcApplnStop ();
            }

            /* Verify that the interface number is 0. */
            if ((evdata & 0xFF00) == 0)
            {
                glAltIntf = evdata & 0xFF;

                /* Configure the endpoints and the DMA channels. */
                if (glAltIntf != 0)
                {
                    /* No need to create channels if Alt. Setting 0 is selected. */
                    CyFxIsoSrcApplnStart ();
                }
            }
            break;

        case CY_U3P_USB_EVENT_RESET:
        case CY_U3P_USB_EVENT_DISCONNECT:
            case CY_U3P_USB_EVENT_CONNECT:
            /* Stop the loop back function. */
            if (glIsApplnActive)
            {
                CyFxIsoSrcApplnStop ();
            }
            CyU3PUsbLPMEnable ();
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
CyBool_t
CyFxIsoSrcApplnLPMRqtCB (
        CyU3PUsbLinkPowerMode link_mode)
{
    return CyTrue;
}


///XXX CNL Added for GPIF
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
                //CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "O");
            	DataOverrunErrors++;            	//XX XXX XXX XXX TODO This happens all the time now probs cause we are debugging???
            	if( KeepDataAlive )
            	{
            		KeepDataAlive--;
            		if( KeepDataAlive == 0 )
            		{
            		    CyU3PGpifDisable( 0 );
            			//Stop session.
            		}
            	}
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
                CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "PID Error:%d(%d)\r\n", CYU3P_GET_PIB_ERROR_TYPE (cbArg),cbArg);
                break;
        }
    }
}



/* This function initializes the USB Module, sets the enumeration descriptors.
 * This function does not start the ISO streaming and this is done only when
 * SET_CONF event is received. */
void
CyFxIsoSrcApplnInit (void)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    //XXX CNL Setup GPIF

    CyU3PPibClock_t         pibClock;
    CyU3PGpioClock_t        gpioClock;
    //CyU3PGpioSimpleConfig_t gpioConfig;
    //CyU3PReturnStatus_t     apiRetStatus = CY_U3P_SUCCESS;

    /* Initialize the p-port block. We are running the PIB Block at 48 MHz. DLL needs to be enabled for Async
     * operation of GPIF. */
    pibClock.clkDiv      = 4;
    pibClock.clkSrc      = CY_U3P_SYS_CLK;
    pibClock.isHalfDiv   = CyFalse;
    pibClock.isDllEnable = CyTrue;
    apiRetStatus = CyU3PPibInit (CyTrue, &pibClock);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "P-port Initialization failed, Error Code=%d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Load the GPIF configuration for our project.. This loads the state machine for interfacing with SRAM */
    apiRetStatus = CyU3PGpifLoad (&CyFxGpifConfig);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "CyU3PGpifLoad failed, Error Code=%d\r\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* callback to see if there is any overflow of data on the GPIF II side*/
    //XXX Why is CYU3P_PIB_ERR_THR0_WR_OVERRUN getting called incessently?
    //CyU3PPibRegisterCallback (PibEventCallback, 0xfffb ); //Don't warn about CYU3P_PIB_ERR_THR0_WR_OVERRUN.  Seems to cause problems.
    CyU3PPibRegisterCallback (PibEventCallback, 0xffff ); //Well, this seems not to break it?
    //DataOverrunErrors


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


    CyU3PDebugPrint( CY_FX_DEBUG_PRIORITY, "Past GPIO Init\r\n");

    //Continue normal start

    /* Start the USB functionality. */
    apiRetStatus = CyU3PUsbStart();
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PUsbStart failed to Start, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* The fast enumeration is the easiest way to setup a USB connection,
     * where all enumeration phase is handled by the library. Only the
     * class / vendor requests need to be handled by the application. */
    CyU3PUsbRegisterSetupCallback(CyFxIsoSrcApplnUSBSetupCB, CyTrue);

    /* Setup the callback to handle the USB events. */
    CyU3PUsbRegisterEventCallback(CyFxIsoSrcApplnUSBEventCB);

    /* Register a callback to handle LPM requests from the USB 3.0 host. */
    CyU3PUsbRegisterLPMRequestCallback(CyFxIsoSrcApplnLPMRqtCB);
    
    /* Set the USB Enumeration descriptors */

    /* Super speed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR, 0, (uint8_t *)CyFxUSB30DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* High speed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, 0, (uint8_t *)CyFxUSB20DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* BOS descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, 0, (uint8_t *)CyFxUSBBOSDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Device qualifier descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, 0, (uint8_t *)CyFxUSBDeviceQualDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set device qualifier descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Super speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBSSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* High speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBHSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB Set Other Speed Descriptor failed, Error Code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Full speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBFSConfigDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB Set Configuration Descriptor failed, Error Code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 0 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)CyFxUSBStringLangIDDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 1 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)CyFxUSBManufactureDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 2 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Connect the USB Pins with super speed operation enabled. */
    apiRetStatus = CyU3PConnectState(CyTrue, CyTrue);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB Connect failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }
}

/* Entry function for the IsoSrcAppThread. */
void
IsoSrcAppThread_Entry (
        uint32_t input)
{
    uint32_t evMask = CYFX_ISOAPP_CTRL_TASK;
    uint32_t evStat;
    CyU3PReturnStatus_t stat;

    /* Initialize the debug module */
    CyFxIsoSrcApplnDebugInit();

    /* Initialize the ISO loop application */
    CyFxIsoSrcApplnInit();

    /* Wait for any vendor specific requests to arrive, and then handle them. */
    for (;;)
    {
        stat = CyU3PEventGet (&glAppEvent, evMask, CYU3P_EVENT_OR_CLEAR, &evStat, CYU3P_WAIT_FOREVER);
        if (stat == CY_U3P_SUCCESS)
        {
            if (evStat & CYFX_ISOAPP_CTRL_TASK)
            {
                uint8_t  bRequest, bReqType;
                uint16_t wLength;
                CyU3PI2cPreamble_t preamble;

                /* Decode the fields from the setup request. */
                bReqType = (glCtrlDat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
                bRequest = ((glCtrlDat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
                wLength  = ((glCtrlDat1 & CY_U3P_USB_LENGTH_MASK)  >> CY_U3P_USB_LENGTH_POS);

                if ((bReqType & CY_U3P_USB_TYPE_MASK) == CY_U3P_USB_VENDOR_RQT)
                {
                    switch (bRequest)
                    {
                    case CYFX_ISOAPP_DOUT_RQT:
                        /* If the user data can fit in our buffer, read it; otherwise, stall the control pipe. */
                        if (wLength <= CYFX_ISOAPP_MAX_EP0LEN)
                        {
                            CyU3PUsbGetEP0Data (wLength, glEp0Buffer, 0);

                            /* Write the data received into the I2C EEPROM. */
                            preamble.length    = 3;
                            preamble.buffer[0] = 0xA0;
                            preamble.buffer[1] = 0x00;
                            preamble.buffer[2] = 0x00;
                            preamble.ctrlMask  = 0x0000;
                            stat = CyU3PI2cTransmitBytes (&preamble, glEp0Buffer, wLength, 0);
                            if (stat != CY_U3P_SUCCESS)
                                CyFxAppErrorHandler (stat);

                            preamble.length = 1;
                            stat = CyU3PI2cWaitForAck (&preamble, 200);
                            if (stat != CY_U3P_SUCCESS)
                                CyFxAppErrorHandler (stat);
                        }
                        else
                            CyU3PUsbStall (0, CyTrue, CyFalse);
                        break;

                    case CYFX_ISOAPP_DIN_RQT:
                        /* If the requested length is less than our buffer size, send data; otherwise, stall the
                           control pipe. */
                        if (wLength <= CYFX_ISOAPP_MAX_EP0LEN)
                        {
                            /* Read data from the EEPROM and send to the host. */
                            preamble.length    = 4;
                            preamble.buffer[0] = 0xA0;
                            preamble.buffer[1] = 0x00;
                            preamble.buffer[2] = 0x00;
                            preamble.buffer[3] = 0xA1;
                            preamble.ctrlMask  = 0x0004;
                            stat = CyU3PI2cReceiveBytes (&preamble, glEp0Buffer, wLength, 0);

                            if (stat == CY_U3P_SUCCESS)
                                CyU3PUsbSendEP0Data (wLength, glEp0Buffer);
                            else
                                CyU3PUsbStall (0, CyTrue, CyFalse);
                        }
                        else
                            CyU3PUsbStall (0, CyTrue, CyFalse);
                        break;

                    case CYFX_ISOAPP_NODATA_RQT:
                        /* Stall the control pipe if the host wants any data, otherwise ack and complete the request. */
                        if (wLength != 0)
                            CyU3PUsbStall (0, CyTrue, CyFalse);
                        else
                        {
                            /* Adding a delay to simulate I2C/SPI commands for sensor control. */
                            CyU3PThreadSleep (2);
                            CyU3PUsbAckSetup ();
                        }
                        break;

                    default: /* unknown request, stall the endpoint. */
                        CyU3PUsbStall (0, CyTrue, CyFalse);
                        break;
                    }
                }
                else
                {
                    CyU3PUsbStall (0, CyTrue, CyFalse); /* Only vendor requests are expected. Stall anything else. */
                }
            }
        }
    }
}

/* Application define function which creates the threads. */
void
CyFxApplicationDefine (
        void)
{
    void *ptr = NULL;
    uint32_t retThrdCreate = CY_U3P_SUCCESS;

    /* Create the event group required to defer vendor specific requests. */
    CyU3PEventCreate (&glAppEvent);
    /* Not expecting any failures here. So, not checking the return value. */

    /* Allocate a buffer for handling control requests. */
    glEp0Buffer = (uint8_t *)CyU3PDmaBufferAlloc (CYFX_ISOAPP_MAX_EP0LEN);
    if (glEp0Buffer == 0)
    {
        while (1);
    }

    /* Allocate the memory for the threads */
    ptr = CyU3PMemAlloc (CY_FX_ISOSRC_THREAD_STACK);

    /* Create the thread for the application */
    retThrdCreate = CyU3PThreadCreate (&isoSrcAppThread,      /* ISO loop App Thread structure */
                          "21:ISO_SRC_MANUAL__OUT",           /* Thread ID and Thread name */
                          IsoSrcAppThread_Entry,              /* ISO loop App Thread Entry function */
                          0,                                  /* No input parameter to thread */
                          ptr,                                /* Pointer to the allocated thread stack */
                          CY_FX_ISOSRC_THREAD_STACK,          /* ISO loop App Thread stack size */
                          CY_FX_ISOSRC_THREAD_PRIORITY,       /* ISO loop App Thread priority */
                          CY_FX_ISOSRC_THREAD_PRIORITY,       /* ISO loop App Thread priority */
                          CYU3P_NO_TIME_SLICE,                /* No time slice for the application thread */
                          CYU3P_AUTO_START                    /* Start the Thread immediately */
                          );

    /* Check the return code */
    if (retThrdCreate != 0)
    {
        /* Thread Creation failed with the error code retThrdCreate */

        /* Add custom recovery or debug actions here */

        /* Application cannot continue */
        /* Loop indefinitely */
        while(1);
    }
}

/*
 * Main function
 */
int
main (void)
{
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    ///Confusing but seems to set it up so the SYS_CLK can be 400 MHz. from https://community.cypress.com/thread/21688
    CyU3PSysClockConfig_t clkCfg = {
                            CyTrue,
                            2, 2, 2,
                            CyFalse,
                            CY_U3P_SYS_CLK
        };
    status = CyU3PDeviceInit (&clkCfg);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* Initialize the caches. Enable both Instruction and Data Caches. */
    status = CyU3PDeviceCacheControl (CyTrue, CyTrue, CyTrue);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* Configure the IO matrix for the device. On the FX3 DVK board, the COM port 
     * is connected to the IO(53:56). This means that either DQ32 mode should be
     * selected or lppMode should be set to UART_ONLY. Here we are choosing
     * UART_ONLY configuration. */
    io_cfg.isDQ32Bit = CyFalse;
    io_cfg.s0Mode = CY_U3P_SPORT_INACTIVE;
    io_cfg.s1Mode = CY_U3P_SPORT_INACTIVE;
    io_cfg.useUart   = CyTrue;
    io_cfg.useI2C    = CyTrue;
    io_cfg.useI2S    = CyFalse;
    io_cfg.useSpi    = CyFalse;
    io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_UART_ONLY;
    /* No GPIOs are enabled. */
    io_cfg.gpioSimpleEn[0]  = 0;
    io_cfg.gpioSimpleEn[1]  = 0;
    io_cfg.gpioComplexEn[0] = 0;
    io_cfg.gpioComplexEn[1] = 0;
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

/* [ ] */

