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
#include <cyu3gpif.h>#include <cyu3pib.h>#include <pib_regs.h>
#include <cyu3gpio.h>
#include "cyu3pib.h"

#include "fast_gpif2.cydsn/cyfxgpif2config.h"
//#include "fast_gpif2.h"
//#include "cyfxgpif2config.h"

CyU3PThread isoSrcAppThread; /* ISO loop application thread structure */

void CyFxIsoSrcApplnInit(void);

#ifdef DMAMULTI
CyU3PDmaMultiChannel glChHandleIsoSrc; /* DMA MANUAL_OUT channel handle */
#else
CyU3PDmaChannel glChHandleIsoSrc; /* DMA MANUAL_OUT channel handle */
#endif

int gpif_is_complex;
CyU3PPibClock_t pibClock;

uint32_t DataOverrunErrors = 0;
uint32_t dmaevent;
uint32_t KeepDataAlive = 0;	//XXX Tricky: The chip seems to hose itself if you let it run with nothing consuming the data.  Do this to make sure we only run for a little while before getting some messages back from the host on the control ports.

CyBool_t glIsApplnActive = CyFalse; /* Whether the loopback application is active or not. */
uint32_t glDMARxCount = 0; /* Counter to track the number of buffers received. */
uint32_t glDMATxCount = 0; /* Counter to track the number of buffers transmitted. */

uint8_t glAltIntf = 0; /* Currently selected Alternate Interface number. */
uint32_t glCtrlDat0 = 0; /* bmRequestType + bRequest + wValue from control request. */
uint32_t glCtrlDat1 = 0; /* wIndex + wLength from control request. */

CyU3PEvent glAppEvent; /* Event group used to defer handling of vendor specific control requests. */
#define CYFX_ISOAPP_CTRL_TASK   1       /* Deferred event flag indicating pending control request. */
#define CYFX_ISOAPP_NODATA_RQT  0xD3    /* Example request with NO data phase. */

/* Application Error Handler */
void CyFxAppErrorHandler(CyU3PReturnStatus_t apiRetStatus /* API return status */
) {
	/* Application failed with the error code apiRetStatus */

	/* Add custom debug or recovery actions here */

	/* Loop Indefinitely */
	for (;;) {
		/* Thread sleep : 100 ms */
		CyU3PThreadSleep(100);
	}
}

/* This function initializes the debug module. The debug prints
 * are routed to the UART and can be seen using a UART console
 * running at 115200 baud rate. */
void CyFxIsoSrcApplnDebugInit(void) {
	CyU3PUartConfig_t uartConfig;
	CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
	CyU3PI2cConfig_t i2cConfig;

	/* Initialize the UART for printing debug messages */
	apiRetStatus = CyU3PUartInit();
	if (apiRetStatus != CY_U3P_SUCCESS) {
		/* Error handling */
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Set UART configuration */
	CyU3PMemSet((uint8_t *) &uartConfig, 0, sizeof(uartConfig));
	uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
	uartConfig.stopBit = CY_U3P_UART_ONE_STOP_BIT;
	uartConfig.parity = CY_U3P_UART_NO_PARITY;
	uartConfig.txEnable = CyTrue;
	uartConfig.rxEnable = CyFalse;
	uartConfig.flowCtrl = CyFalse;
	uartConfig.isDma = CyTrue;

	apiRetStatus = CyU3PUartSetConfig(&uartConfig, 0);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Set the UART transfer to a really large value. */
	apiRetStatus = CyU3PUartTxSetBlockXfer(0xFFFFFFFF);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Initialize the debug module. */
	apiRetStatus = CyU3PDebugInit(CY_U3P_LPP_SOCKET_UART_CONS, 8);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Initialize and configure the I2C block. */
	apiRetStatus = CyU3PI2cInit();
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyFxAppErrorHandler(apiRetStatus);
	}

	CyU3PMemSet((uint8_t *) &i2cConfig, 0, sizeof(i2cConfig));
	i2cConfig.bitRate = 100000;
	i2cConfig.busTimeout = 0xFFFFFFFF;
	i2cConfig.dmaTimeout = 0xFFFF;
	i2cConfig.isDma = CyFalse;
	apiRetStatus = CyU3PI2cSetConfig(&i2cConfig, NULL);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyFxAppErrorHandler(apiRetStatus);
	}
}

void DMACallback(CyU3PDmaChannel *chHandle, /* Handle to the DMA channel. */
CyU3PDmaCbType_t type, /* Callback type.             */
CyU3PDmaCBInput_t *input) /* Callback status.           */
{
	dmaevent++;

	//  CyU3PDebugPrint (4, "DMA EVENT\r\n", type);

}

/* This function starts the ISO loop application. This is called
 * when a SET_CONF event is received from the USB host. The endpoints
 * are configured and the DMA pipe is setup in this function. */
void CyFxIsoSrcApplnStart(void) {
	uint16_t size = 0;
	CyU3PEpConfig_t epCfg;
	//CyU3PDmaBuffer_t buf_p;
	CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
	CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();
	uint8_t isoPkts = 1;

	CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
			"Starting initialization speed = %d\r\n", usbSpeed);

	/* First identify the usb speed. Once that is identified,
	 * create a DMA channel and start the transfer on this. */

	/* Based on the Bus Speed configure the endpoint packet size */
	switch (usbSpeed) {
	case CY_U3P_FULL_SPEED:
		size = 64;
		isoPkts = 1; /* One packet per frame. */
		break;

	case CY_U3P_HIGH_SPEED:
		size = 1024;
		isoPkts = CY_FX_ISO_PKTS; /* CY_FX_ISO_PKTS packets per microframe. */
		break;

	case CY_U3P_SUPER_SPEED:
		size = 1024;
		if (glAltIntf == 1)
			isoPkts = 1; /* One burst per microframe. */
		else
			isoPkts = CY_FX_ISO_PKTS; /* CY_FX_ISO_PKTS bursts per microframe. */
		break;

	default:
		CyU3PDebugPrint(4, "Error! Invalid USB speed.\n");
		CyFxAppErrorHandler(CY_U3P_ERROR_FAILURE);
		break;
	}

	CyU3PMemSet((uint8_t *) &epCfg, 0, sizeof(epCfg));
	epCfg.enable = CyTrue;
	epCfg.epType = CY_U3P_USB_EP_ISO;
	epCfg.burstLen = (usbSpeed == CY_U3P_SUPER_SPEED) ? (CY_FX_ISO_BURST) : 1;
	epCfg.streams = 0;
	epCfg.pcktSize = size;
	epCfg.isoPkts = isoPkts;

	/* Consumer endpoint configuration */
	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\r\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Flush the endpoint memory */
	CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

	/* Only the EP needs to be configured with the correct parameters. The DMA channel can always be prepared
	 to allow the greatest bandwidth possible.
	 */

#ifndef DMAMULTI
	CyU3PDmaChannelConfig_t dmaCfg;

	dmaCfg.size = ((size + 0x0F) & ~0x0F);
	if (usbSpeed != CY_U3P_FULL_SPEED)
	{
		dmaCfg.size *= CY_FX_ISO_PKTS;
	}

	/* Multiply the buffer size with the burst value for performance improvement. */
	//dmaCfg.size          *= CY_FX_ISO_BURST;//XXX CNL This should be DMA_OUT_BUF_SIZE shouldn't it?
	//dmaCfg.size           = DMA_IN_BUF_SIZE;
	dmaCfg.size = 32768;//Seems to work here and 32768, but smaller values seem to drop data, a lot.
	dmaCfg.count = CY_FX_ISOSRC_DMA_BUF_COUNT;//3
	dmaCfg.prodSckId = CY_U3P_PIB_SOCKET_0;
	dmaCfg.consSckId = CY_FX_EP_CONSUMER_SOCKET;
	dmaCfg.dmaMode = CY_U3P_DMA_MODE_BYTE;//Is this right?  Changing it doesn't seem to effect anything.
	dmaCfg.notification = 0;//0xffffff; //CY_U3P_DMA_CB_CONS_EVENT;
	dmaCfg.cb = DMACallback;//This never seems to be called.  Could it need CY_U3P_DMA_TYPE_AUTO_SIGNAL?
	dmaCfg.prodHeader = 0;
	dmaCfg.prodFooter = 0;
	dmaCfg.consHeader = 0;
	dmaCfg.prodAvailCount = 0;

	CyU3PDebugPrint (4, "DMA Config Size = %d\r\n", dmaCfg.size);

	//apiRetStatus = CyU3PDmaChannelCreate (&glChHandleIsoSrc, CY_U3P_DMA_TYPE_MANUAL_OUT, &dmaCfg); XXX CNL
	apiRetStatus = CyU3PDmaChannelCreate (&glChHandleIsoSrc, CY_U3P_DMA_TYPE_AUTO, &dmaCfg);
#else

	CyU3PDmaMultiChannelConfig_t dmaCfg;
	memset((void*) &dmaCfg, 0, sizeof(dmaCfg));
	dmaCfg.size = MULTI_CHANNEL_XFER_SIZE; //Seems to work here and 16384, but smaller values seem to drop data, a lot.
	dmaCfg.count = CY_FX_ISOSRC_DMA_BUF_COUNT;
	dmaCfg.prodSckId[0] = CY_U3P_PIB_SOCKET_0;
	dmaCfg.prodSckId[1] = CY_U3P_PIB_SOCKET_1;
	dmaCfg.validSckCount = 2;
	dmaCfg.consSckId[0] = CY_FX_EP_CONSUMER_SOCKET;
	dmaCfg.dmaMode = CY_U3P_DMA_MODE_BYTE; //Is this right?  Changing it doesn't seem to effect anything.
	dmaCfg.notification = 0; //CY_U3P_DMA_CB_PROD_EVENT; //CY_U3P_DMA_CB_CONS_EVENT;
	dmaCfg.cb = 0; //DMACallback; //This never seems to be called.  Could it need CY_U3P_DMA_TYPE_AUTO_SIGNAL?
	dmaCfg.prodHeader = 0;
	dmaCfg.prodFooter = 0;
	dmaCfg.consHeader = 0;
	dmaCfg.prodAvailCount = 0;

	apiRetStatus = CyU3PDmaMultiChannelCreate(&glChHandleIsoSrc,
			CY_U3P_DMA_TYPE_AUTO_MANY_TO_ONE, &dmaCfg);
	CyU3PGpifSocketConfigure(0, CY_U3P_PIB_SOCKET_0, 1, CyTrue, 1);
	CyU3PGpifSocketConfigure(1, CY_U3P_PIB_SOCKET_1, 1, CyTrue, 1);
#endif

	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"CyU3PDmaMultiChannelCreate failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

#ifndef DMAMULTI
	apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandleIsoSrc, 0 ); //0 = unlimited.
#else
	apiRetStatus = CyU3PDmaMultiChannelSetXfer(&glChHandleIsoSrc, 0, 0); //0 = unlimited.
#endif

	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "CyU3PDmaChannelSetXfer Failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Update the flag so that the application thread is notified of this. */
	glIsApplnActive = CyTrue;
}

/* This function stops the ISO loop application. This shall be called whenever
 * a RESET or DISCONNECT event is received from the USB host. The endpoints are
 * disabled and the DMA pipe is destroyed by this function. */
void CyFxIsoSrcApplnStop(void) {
	CyU3PEpConfig_t epCfg;
	CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

	CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY, "De-initializing...\r\n");

	//Stop the GPIF bus.
	CyU3PGpifDisable(0);

	CyU3PDmaMultiChannelReset(&glChHandleIsoSrc);
	/* Flush the endpoint memory */
	CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);
	CyU3PUsbResetEp(CY_FX_EP_CONSUMER);
	CyU3PDmaMultiChannelSetXfer(&glChHandleIsoSrc, 2, CY_U3P_PIB_SOCKET_0);

	/* Update the flag so that the application thread is notified of this. */
	glIsApplnActive = CyFalse;

	/* Destroy the channels */
#ifdef DMAMULTI
	CyU3PDmaMultiChannelDestroy(&glChHandleIsoSrc);
#else
	CyU3PDmaChannelDestroy (&glChHandleIsoSrc);
#endif

	/* Flush the endpoint memory */
	CyU3PUsbFlushEp(CY_FX_EP_CONSUMER);

	/* Disable endpoints. */
	CyU3PMemSet((uint8_t *) &epCfg, 0, sizeof(epCfg));
	epCfg.enable = CyFalse;

	/* Consumer endpoint configuration. */
	apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CONSUMER, &epCfg);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "CyU3PSetEpConfig failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}
}


CyU3PGpioSimpleConfig_t cfgIn = { 1, 0, 0, 1, 0 };
CyU3PGpioSimpleConfig_t cfgLo = { 0, 1, 0, 0, 0 };
CyU3PGpioSimpleConfig_t cfgHi = { 1, 0, 1, 0, 0 };

#define SDAP par1
#define SCLP par2
//#define RELEASE(x) { CyU3PGpioSetSimpleConfig( x, &cfgIn ); }
//#define HOLD_LO(x) { CyU3PGpioSetSimpleConfig( x, &cfgLo ); }
void I2CDELAY() { 	int i; for( i = 0; i < 200; i++ ) asm volatile( "nop" ); }
void RELEASE( int x )
{
	CyU3PGpioSetSimpleConfig( x & 0x7f, (x&0x80)?(&cfgHi):(&cfgIn) );
	I2CDELAY();
}

void HOLD_LO( int x )
{
	CyU3PGpioSetSimpleConfig( x & 0x7f, &cfgLo );
	I2CDELAY();
}


/* Callback to handle the USB setup requests. */
CyBool_t CyFxIsoSrcApplnUSBSetupCB(	uint32_t setupdat0, /* SETUP Data 0 */
									uint32_t setupdat1 /* SETUP Data 1 */
) {
	/* Fast enumeration is used. Only requests addressed to the interface, class,
	 * vendor and interface/endpoint control requests are received by this function.
	 */

	uint8_t bRequest, bReqType;
	uint8_t bType, bTarget;
	uint16_t wValue;
	CyBool_t isHandled = CyFalse;

	/* Decode the fields from the setup request. */
	bReqType = (setupdat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
	bType = (bReqType & CY_U3P_USB_TYPE_MASK);
	bTarget = (bReqType & CY_U3P_USB_TARGET_MASK);
	bRequest =
			((setupdat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
	wValue = ((setupdat0 & CY_U3P_USB_VALUE_MASK) >> CY_U3P_USB_VALUE_POS);

	if (bType == CY_U3P_USB_STANDARD_RQT) {
		/* Handle SET_FEATURE(FUNCTION_SUSPEND) and CLEAR_FEATURE(FUNCTION_SUSPEND)
		 * requests here. It should be allowed to pass if the device is in configured
		 * state and failed otherwise. */
		if ((bTarget == CY_U3P_USB_TARGET_INTF)
				&& ((bRequest == CY_U3P_USB_SC_SET_FEATURE)
						|| (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE))
				&& (wValue == 0)) {
			if (glIsApplnActive)
				CyU3PUsbAckSetup();
			else
				CyU3PUsbStall(0, CyTrue, CyFalse);

			isHandled = CyTrue;
		}
	}

	/* Defer handling all vendor specific control requests to the application thread. */
	if (bType == CY_U3P_USB_VENDOR_RQT) {
		glCtrlDat0 = setupdat0;
		glCtrlDat1 = setupdat1;
		isHandled = CyTrue; /* Tell the driver that this request has been handled, so that
		 the driver does not take any default action. */

		//XXX NOTE: Could handle special requests in here if really low latency is needed.
		if (bRequest == 0xaa) {
			uint32_t sendback[10];
			sendback[0] = 0xaabbccdd;
			sendback[1] = DataOverrunErrors;
			sendback[2] = dmaevent;
			CyU3PUsbSendEP0Data(12, (uint8_t*) sendback);  //Sends back "hello"
			if (KeepDataAlive == 0) {
				CyFxIsoSrcApplnStart();
				CyU3PGpifSMStart(START, ALPHA_START);
			}
			KeepDataAlive = 1200 * 2;
		} else if (bRequest == 0xbb) {
			uint32_t sendback[10];
			sendback[0] = 0xaabbccdd;
			sendback[1] = DataOverrunErrors;
			sendback[2] = dmaevent;
			CyU3PUsbSendEP0Data(12, (uint8_t*) sendback);  //Sends back "hello"
			CyFxIsoSrcApplnStop();
			KeepDataAlive = 0;
		} else if( bRequest == 0xcc ) {
			uint8_t buffer[512];
			uint16_t readCount = 0;
			CyU3PReturnStatus_t st = CyU3PUsbGetEP0Data( 512, buffer, &readCount );
			CyU3PDebugPrint( 4, "CyU3PUsbGetEP0Data(...) = %d -> readCount: %d [%d]\r\n", st, readCount, buffer[0] );
			isHandled = CyTrue;
		} else {
			/* Send an event to the thread, asking for this control request to be handled. */
			CyU3PEventSet(&glAppEvent, CYFX_ISOAPP_CTRL_TASK, CYU3P_EVENT_OR);
		}
	}
	return isHandled;
}

/* This is the callback function to handle the USB events. */
void CyFxIsoSrcApplnUSBEventCB(CyU3PUsbEventType_t evtype, /* Event type */
uint16_t evdata /* Event data */
) {
	switch (evtype) {
	/* In this ISO application, channels are created and destroyed when an interface is selected.
	 If Alt Setting 0 is selected for interface 0, channels can be freed up.
	 If any other Alt Setting is selected, the channels are created afresh.
	 */
	case CY_U3P_USB_EVENT_SETINTF:
		/* Make sure to remove any previous configuration, before starting afresh. */
		if (glIsApplnActive) {
			CyFxIsoSrcApplnStop();
		}

		/* Verify that the interface number is 0. */
		if ((evdata & 0xFF00) == 0) {
			glAltIntf = evdata & 0xFF;

			/* Configure the endpoints and the DMA channels. */
			if (glAltIntf != 0) {
				/* No need to create channels if Alt. Setting 0 is selected. */
				CyFxIsoSrcApplnStart();
			}
		}
		break;

	case CY_U3P_USB_EVENT_RESET:
	case CY_U3P_USB_EVENT_DISCONNECT:
	case CY_U3P_USB_EVENT_CONNECT:
		/* Stop the loop back function. */
		if (glIsApplnActive) {
			CyFxIsoSrcApplnStop();
		}
		CyU3PUsbLPMEnable();
		break;
	case CY_U3P_USB_EVENT_SOF_ITP:
	{
		//volatile uint32_t * LPPROT_ITP_TIMESTAMP = (volatile uint32_t*)0xE0033434;
		// BIG NOTE - we output a copy of the ITP's 8th bit on GPIO 29.
		volatile uint32_t * LPPROT_FRAMECNT = (volatile uint32_t*)0xE0033428;
		uint8_t val = ((*(LPPROT_FRAMECNT))>>8)&1;
		CyU3PGpioSimpleSetValue( 29, val );
		break;
	}
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
CyBool_t CyFxIsoSrcApplnLPMRqtCB(CyU3PUsbLinkPowerMode link_mode) {
	return CyTrue;
}

///XXX CNL Added for GPIF
/* P-Port Event Callback. Used to identify error events thrown by the GPIF block.
 * We don't expect errors to happen, and only print a message in case one is reported.
 */
static void PibEventCallback(CyU3PPibIntrType cbType, uint16_t cbArg) {
	if (cbType == CYU3P_PIB_INTR_ERROR) {
		switch (CYU3P_GET_PIB_ERROR_TYPE(cbArg)) {
		case CYU3P_PIB_ERR_THR2_WR_OVERRUN:
		case CYU3P_PIB_ERR_THR1_WR_OVERRUN:
		case CYU3P_PIB_ERR_THR0_WR_OVERRUN:
			CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY, "%d\r\n", KeepDataAlive);
			DataOverrunErrors++;
			if (KeepDataAlive) {
				KeepDataAlive--;
				if (KeepDataAlive == 0) {
					CyFxIsoSrcApplnStop();
					//Stop session.
				}
			}
			break;
		case CYU3P_PIB_ERR_THR3_WR_OVERRUN:
			CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
					"CYU3P_PIB_ERR_THR3_WR_OVERRUN\r\n");
			break;
		case CYU3P_PIB_ERR_THR0_RD_UNDERRUN:
			CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
					"CYU3P_PIB_ERR_THR0_RD_UNDERRUN\r\n");
			break;
		case CYU3P_PIB_ERR_THR1_RD_UNDERRUN:
			CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
					"CYU3P_PIB_ERR_THR1_RD_UNDERRUN\r\n");
			break;
		case CYU3P_PIB_ERR_THR2_RD_UNDERRUN:
			CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
					"CYU3P_PIB_ERR_THR2_RD_UNDERRUN\r\n");
			break;
		case CYU3P_PIB_ERR_THR3_RD_UNDERRUN:
			CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
					"CYU3P_PIB_ERR_THR3_RD_UNDERRUN\r\n");
			break;
		default:
			CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY, "PID Error:%d(%d)\r\n",
					CYU3P_GET_PIB_ERROR_TYPE(cbArg), cbArg);
			break;
		}
	}
	CyU3PUsbEnableITPEvent( CyTrue );
}

/* This function initializes the USB Module, sets the enumeration descriptors.
 * This function does not start the ISO streaming and this is done only when
 * SET_CONF event is received. */
void CyFxIsoSrcApplnInit(void) {
	CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

	//XXX CNL Setup GPIF

	CyU3PGpioClock_t gpioClock;
	//CyU3PGpioSimpleConfig_t gpioConfig;
	//CyU3PReturnStatus_t     apiRetStatus = CY_U3P_SUCCESS;

	pibClock.clkDiv = 8; //~400 MHz / 8.  or 400 / 8  --- or 384 / 4 or 384 / 8
	pibClock.clkSrc = CY_U3P_SYS_CLK;
	pibClock.isHalfDiv = CyFalse; //Adds 0.5 to divisor
	pibClock.isDllEnable = CyTrue;	//For async or master-mode
	apiRetStatus = CyU3PPibInit(CyTrue, &pibClock);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
				"P-port Initialization failed, Error Code=%d\r\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Load the GPIF configuration for our project.. This loads the state machine for interfacing with SRAM */
	apiRetStatus = CyU3PGpifLoad(&CyFxGpifConfig);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
				"CyU3PGpifLoad failed, Error Code=%d\r\n", apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* callback to see if there is any overflow of data on the GPIF II side*/
	//XXX Why is CYU3P_PIB_ERR_THR0_WR_OVERRUN getting called incessently?
	// CyU3PPibRegisterCallback (PibEventCallback, 0xfffb ); //Don't warn about CYU3P_PIB_ERR_THR0_WR_OVERRUN.  Seems to cause problems.
	CyU3PPibRegisterCallback(PibEventCallback, 0xffff); //Well, this seems not to break it?
	//DataOverrunErrors

	/* Initialize the GPIO module. This is used to update the indicator LED. */
	gpioClock.fastClkDiv = 2;
	gpioClock.slowClkDiv = 0;
	gpioClock.simpleDiv = CY_U3P_GPIO_SIMPLE_DIV_BY_2;
	gpioClock.clkSrc = CY_U3P_SYS_CLK;
	gpioClock.halfDiv = 0;

	apiRetStatus = CyU3PGpioInit(&gpioClock, 0);
	if (apiRetStatus != 0) {
		/* Error Handling */
		CyU3PDebugPrint(CY_FX_DEBUG_PRIORITY,
				"CyU3PGpioInit failed, error code = %d\r\n", apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	//19 = CTRL[2]
	CyU3PGpioSimpleConfig_t cfg =
	{
		.driveHighEn = 1 & 0x01,
		.driveLowEn = 1 & 0x01,
		.inputEn = 0 & 0x01,
		.outValue = 1 & 0x01,
		.intrMode = CY_U3P_GPIO_NO_INTR,
	};
	CyU3PDeviceGpioOverride( 29, CyTrue );
	CyU3PGpioSetSimpleConfig( 29, &cfg);

	CyU3PDebugPrint( CY_FX_DEBUG_PRIORITY, "Past GPIO Init\r\n");

	//Continue normal start

	/* Start the USB functionality. */
	apiRetStatus = CyU3PUsbStart();
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "CyU3PUsbStart failed to Start, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* The fast enumeration is the easiest way to setup a USB connection,
	 * where all enumeration phase is handled by the library. Only the
	 * class / vendor requests need to be handled by the application. */
	CyU3PUsbRegisterSetupCallback(CyFxIsoSrcApplnUSBSetupCB, CyTrue);

	/* Setup the callback to handle the USB events. */
	CyU3PUsbRegisterEventCallback(CyFxIsoSrcApplnUSBEventCB);

	CyU3PUsbEnableITPEvent( CyTrue );

	/* Register a callback to handle LPM requests from the USB 3.0 host. */
	CyU3PUsbRegisterLPMRequestCallback(CyFxIsoSrcApplnLPMRqtCB);

	CyU3PDebugPrint( CY_FX_DEBUG_PRIORITY, "Setting USB Descriptors\r\n");
	/* Set the USB Enumeration descriptors */

	/* Super speed device descriptor. */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR, 0,
			(uint8_t *) CyFxUSB30DeviceDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB set device descriptor failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* High speed device descriptor. */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, 0,
			(uint8_t *) CyFxUSB20DeviceDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB set device descriptor failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* BOS descriptor */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, 0,
			(uint8_t *) CyFxUSBBOSDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB set configuration descriptor failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Device qualifier descriptor */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, 0,
			(uint8_t *) CyFxUSBDeviceQualDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB set device qualifier descriptor failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Super speed configuration descriptor */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR, 0,
			(uint8_t *) CyFxUSBSSConfigDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB set configuration descriptor failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* High speed configuration descriptor */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, 0,
			(uint8_t *) CyFxUSBHSConfigDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB Set Other Speed Descriptor failed, Error Code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Full speed configuration descriptor */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, 0,
			(uint8_t *) CyFxUSBFSConfigDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB Set Configuration Descriptor failed, Error Code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* String descriptor 0 */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0,
			(uint8_t *) CyFxUSBStringLangIDDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB set string descriptor failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* String descriptor 1 */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1,
			(uint8_t *) CyFxUSBManufactureDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB set string descriptor failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* String descriptor 2 */
	apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2,
			(uint8_t *) CyFxUSBProductDscr);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4,
				"USB set string descriptor failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	/* Connect the USB Pins with super speed operation enabled. */
	apiRetStatus = CyU3PConnectState(CyTrue, CyTrue);
	if (apiRetStatus != CY_U3P_SUCCESS) {
		CyU3PDebugPrint(4, "USB Connect failed, Error code = %d\n",
				apiRetStatus);
		CyFxAppErrorHandler(apiRetStatus);
	}

	CyU3PDebugPrint( CY_FX_DEBUG_PRIORITY, "USB Pins Connected\r\n");
}

/* Entry function for the IsoSrcAppThread. */
void IsoSrcAppThread_Entry(uint32_t input) {
	uint32_t evMask = CYFX_ISOAPP_CTRL_TASK;
	uint32_t evStat;
	CyU3PReturnStatus_t stat;

	/* Initialize the debug module */
	CyFxIsoSrcApplnDebugInit();

	/* Initialize the ISO loop application */
	CyFxIsoSrcApplnInit();

	/* Wait for any vendor specific requests to arrive, and then handle them. */
	for (;;) {
		stat = CyU3PEventGet(&glAppEvent, evMask, CYU3P_EVENT_OR_CLEAR, &evStat,
				CYU3P_WAIT_FOREVER);
		if (stat == CY_U3P_SUCCESS) {
			if (evStat & CYFX_ISOAPP_CTRL_TASK) {
				uint8_t bRequest, bReqType;
				uint16_t wLength, wValue;
				CyU3PI2cPreamble_t preamble;

				/* Decode the fields from the setup request. */
				bReqType = (glCtrlDat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
				bRequest = ((glCtrlDat0 & CY_U3P_USB_REQUEST_MASK)
						>> CY_U3P_USB_REQUEST_POS);
				wLength = ((glCtrlDat1 & CY_U3P_USB_LENGTH_MASK)
						>> CY_U3P_USB_LENGTH_POS);
				wValue = ((glCtrlDat0 & CY_U3P_USB_VALUE_MASK) >> CY_U3P_USB_VALUE_POS);

				uint16_t wIndex = glCtrlDat1 & 0xffff;
				uint8_t opcode = wValue & 0xff;
				uint8_t par1 = wValue >> 8;
				uint8_t par2 = wIndex & 0xff;
				uint8_t par3 = wIndex >> 8;

				if ((bReqType & CY_U3P_USB_TYPE_MASK) == CY_U3P_USB_VENDOR_RQT) {
					switch (bRequest) {
					case 0xDD:  //Special commands.
					{
						uint8_t reply[64];
						int replylen = 0;
						switch( opcode )
						{
						case 0x00:	//NOP
							break;
						case 0x01: //GPIO Config.
						{
							//25 = CTRL[8] --> See https://github.com/baidu/boteye_sensor/blob/302fc586ae361b0d0e49118a67dc88c3a0fa88a1/firmware/include/fx3_bsp.h

							//Usage: par1 = which GPIO
							//       par2 = 0b pull down / pull up       outValue / in enable / drive low en / drive high en
							CyU3PGpioSimpleConfig_t cfg =
							{
								.driveHighEn = (par2>>0) & 0x01,
								.driveLowEn = (par2>>1) & 0x01,
								.inputEn = (par2>>2) & 0x01,
								.outValue = (par2>>3) & 0x01,
								.intrMode = CY_U3P_GPIO_NO_INTR,
							};
							CyU3PDeviceGpioOverride( par1 & 0x7f, CyTrue );
							CyU3PGpioSetIoMode( par1 & 0x7f, (CyU3PGpioIoMode_t)(( par2 >> 4 ) & 3) );
							CyU3PReturnStatus_t apiRetStatus = CyU3PGpioSetSimpleConfig( par1 & 0x7f, &cfg);
							CyU3PDebugPrint (4, "CyU3PGpioSetSimpleConfig = %d {%d = %d %d %d %d }\n", apiRetStatus, par1, cfg.driveHighEn, cfg.driveLowEn, cfg.inputEn, cfg.outValue );
							reply[0] = apiRetStatus;
							replylen = 1;
							break;
						}
						case 0x02: //GPIO Read.
						{
							CyBool_t v;
							reply[0] = CyU3PGpioSimpleGetValue( par1 & 0x7f, &v );
							reply[1] = v;
							replylen = 2;
							break;
						}
						case 0x03: //GPIO Write
						{
							reply[1] = CyU3PGpioSimpleSetValue( par1 & 0x7f, par2 );
							replylen = 1;
							break;
						}

						case 0x04: //Send I2C Start
						{
							CyU3PDeviceGpioOverride( SCLP & 0x7f, CyTrue );
							CyU3PDeviceGpioOverride( SDAP & 0x7f, CyTrue );
							CyU3PGpioSetIoMode( SCLP & 0x7f, CY_U3P_GPIO_IO_MODE_WPU );
							CyU3PGpioSetIoMode( SDAP & 0x7f, CY_U3P_GPIO_IO_MODE_WPU );

							//Par1 (SDA), Par2 (SCL) = GPIOs for I2C start.
							RELEASE(SCLP);
							HOLD_LO(SDAP);
							HOLD_LO(SCLP);
							break;
						}
						case 0x05: //Send I2C Stop
						{
							CyU3PDeviceGpioOverride( SCLP & 0x7f, CyTrue );
							CyU3PDeviceGpioOverride( SDAP & 0x7f, CyTrue );
							CyU3PGpioSetIoMode( SCLP & 0x7f, CY_U3P_GPIO_IO_MODE_WPU );
							CyU3PGpioSetIoMode( SDAP & 0x7f, CY_U3P_GPIO_IO_MODE_WPU );

							//Par1 (SDA), Par2 (SCL) = GPIOs for I2C start.
							HOLD_LO( SDAP );
							RELEASE( SCLP );
							RELEASE( SDAP );
							break;
						}
						case 0x06: //Send I2C Byte
						{
							CyU3PDeviceGpioOverride( SCLP & 0x7f, CyTrue );
							CyU3PDeviceGpioOverride( SDAP & 0x7f, CyTrue );
							CyU3PGpioSetIoMode( SCLP & 0x7f, CY_U3P_GPIO_IO_MODE_WPU );
							CyU3PGpioSetIoMode( SDAP & 0x7f, CY_U3P_GPIO_IO_MODE_WPU );

							//Par1 (SDA), Par2 (SCL) = GPIOs for I2C start.
							replylen = 2;
							HOLD_LO( SCLP );
							int i;
							for( i = 0; i < 8; i++ )
							{
								if( par3 & 0x80 )
								{
									RELEASE( SDAP );
								}
								else
								{
									HOLD_LO( SDAP );
								}
								par3<<=1;
								RELEASE( SCLP );
								I2CDELAY();
								HOLD_LO( SCLP );
							}
							RELEASE( SDAP & 0x7F ); //Actually tristate!
							RELEASE( SCLP );
							I2CDELAY();
							I2CDELAY();
							I2CDELAY();
							CyBool_t v;
							reply[0] = CyU3PGpioSimpleGetValue( SDAP, &v );
							reply[1] = v;
							HOLD_LO( SCLP );
							RELEASE( SDAP ); // Potentially drive.
							break;
						}
						case 0x07: //Get I2C Byte
						case 0x08: //Get I2C Byte w/NAK
						{
							CyU3PDeviceGpioOverride( SCLP & 0x7f, CyTrue );
							CyU3PDeviceGpioOverride( SDAP & 0x7f, CyTrue );
							CyU3PGpioSetIoMode( SCLP & 0x7f, CY_U3P_GPIO_IO_MODE_WPU );
							CyU3PGpioSetIoMode( SDAP & 0x7f, CY_U3P_GPIO_IO_MODE_WPU );

							replylen = 2;
							reply[0] = 0;
							RELEASE( SDAP & 0x7f ); //Actually tristate
							uint8_t rv = 0;
							int i;
							for( i = 0; i < 8; i++ )
							{
								RELEASE( SCLP );
								I2CDELAY();
								I2CDELAY();
								I2CDELAY();
								rv <<= 1;
								CyBool_t v;
								reply[0] |= CyU3PGpioSimpleGetValue( SDAP & 0x7f, &v );
								rv |= !!v;
								HOLD_LO( SCLP );
							}
							reply[1] = rv;

							I2CDELAY();

							if( opcode == 0x08 )
							{
								RELEASE( SDAP );
							}
							else
							{
								HOLD_LO( SDAP );
							}
							I2CDELAY();
							RELEASE( SCLP );
							I2CDELAY();
							HOLD_LO( SCLP );
							break;
						}
						case 0x10:
						{
							CyU3PGpifDisable(CyFalse);
							CyU3PPibDeInit();
							//Reconfigure Clock
							pibClock.clkDiv = par1; //~400 MHz / 4.  or 400 / 8  --- or 384 / 4 or 384 / 8
							pibClock.clkSrc = CY_U3P_SYS_CLK;
							pibClock.isHalfDiv = !!(par2 & 1); 		//Adds 0.5 to divisor
							pibClock.isDllEnable = !!(par2 & 2);	//For async or master-mode
							reply[0] = CyU3PPibInit(CyTrue, &pibClock);
							//CyU3PDebugPrint (4, "CyU3PPibInit( %d %d %d)\n", par1, par2, par3 );

							if( par3 == 1 )
							{
								gpif_is_complex = 1;
								extern int ComplexGPIFLoad();
								reply[1] = ComplexGPIFLoad();
							}
							else
							{
								gpif_is_complex = 0;
								reply[1] = CyU3PGpifLoad(&CyFxGpifConfig);
							}

							replylen = 2;
							break;
						}
						case 0x11:
						{
							//https://community.cypress.com/t5/Knowledge-Base-Articles/Configuring-EZ-USB-FX3-GPIF-II-DLL-KBA210733/ta-p/247793
							//https://community.cypress.com/t5/USB-Superspeed-Peripherals/EZ-USB-FX3-GPIF-II-synchronous-master-supported-clock-rates-and/m-p/263978
			//				CyU3PGpifDisable(CyFalse);
			//				CyU3PPibDeInit();
			//				reply[0] = CyU3PPibInit(CyTrue, &pibClock);
						//	reply[1] = CyU3PPibDllConfigure( CYU3P_PIB_DLL_MASTER,
						//			par1*2, par2 & 0xf, (par2 >> 4) & 0xf, par3, CyFalse );
							#define CY_FX3_PIB_DLL_CORE_PHASE_POS   (4)                             /* Position of core clock phase field. */
							#define CY_FX3_PIB_DLL_SYNC_PHASE_POS   (8)                             /* Position of sync clock phase field. */
							#define CY_FX3_PIB_DLL_OP_PHASE_POS     (12)                            /* Position of output clock phase field. */
							#define CY_FX3_PIB_DLL_ENABLE  1
							#define CY_FX3_PIB_DLL_HIGH_FREQ  2
							#define CY_FX3_PIB_DLL_RESET_N (1<<30)

							volatile uint32_t * CY_FX3_PIB_DLL_CTRL_REG =  (volatile uint32_t *)0xE0010028;

							*CY_FX3_PIB_DLL_CTRL_REG &= ~(CY_FX3_PIB_DLL_ENABLE);

							I2CDELAY();
							I2CDELAY();
							I2CDELAY();

							int corePhase = par2 & 0xf;
							int syncPhase = (par2 >> 4) & 0xf;
							int opPhase = par3 & 0xf;
							int phaseDelay = par1*2;
							int enable = ( par3 >> 4 ) & 1;
							int high = ( par3 >> 4 ) & 2;
							int slave_mode = (enable)?0:7;
							*CY_FX3_PIB_DLL_CTRL_REG = (
								((corePhase & 0x0F) << CY_FX3_PIB_DLL_CORE_PHASE_POS) |
								((syncPhase & 0x0F) << CY_FX3_PIB_DLL_SYNC_PHASE_POS) |
								((opPhase & 0x0F)   << CY_FX3_PIB_DLL_OP_PHASE_POS) |
								high |
								enable |
								(phaseDelay<<17) |
								(slave_mode<<27) |
								0x80000000
							);

							*CY_FX3_PIB_DLL_CTRL_REG &= ~(CY_FX3_PIB_DLL_RESET_N);

							I2CDELAY();
							I2CDELAY();
							I2CDELAY();

							/* Clear Reset */

							*CY_FX3_PIB_DLL_CTRL_REG |= CY_FX3_PIB_DLL_RESET_N;

							I2CDELAY();
							I2CDELAY();
							I2CDELAY();
							reply[3] = 255;
							reply[4] = par1;
							reply[5] = par2;
							reply[6] = par3;
							reply[7] = 255;
							replylen = 8;

							break;
						}
						default:
						{
							CyU3PDebugPrint (4, "UDC/%d %d %d %d/\n", bReqType, bRequest, wLength, wValue );
							break;
						}
						}
						CyU3PUsbSendEP0Data(replylen, (uint8_t*) reply);  //Sends back reply
						//CyU3PDebugPrint (4, "DD { %d %d %d %d } Len = %d G1: %d WV: %d\n", opcode, par1, par2, par3, replylen, glCtrlDat0, wValue );
						//
						break;
					}

					case CYFX_ISOAPP_NODATA_RQT:
						/* Stall the control pipe if the host wants any data, otherwise ack and complete the request. */
						if (wLength != 0)
							CyU3PUsbStall(0, CyTrue, CyFalse);
						else {
							/* Adding a delay to simulate I2C/SPI commands for sensor control. */
							CyU3PThreadSleep(2);
							CyU3PUsbAckSetup();
						}
						break;

					case 0xbc: // Write I2C Flash (Must be 256-byte aligned); Or if write size is 0 write reboots.
						if( bReqType & 0x80 )
						{
							uint8_t buffer[wLength];
							uint32_t readAddress = wIndex + (((uint32_t)wValue) << 16);
							/* Read data from the EEPROM and send to the host. */
							preamble.length = 4;
							preamble.buffer[0] = 0xA0 | ( (readAddress>>15) & 6);
							preamble.buffer[1] = (readAddress>>8)&0xff;
							preamble.buffer[2] = readAddress&0xff;
							preamble.buffer[3] = 0xA1;
							preamble.ctrlMask = 0x0004;
							stat = CyU3PI2cReceiveBytes( &preamble, buffer, wLength, 0 );
							CyU3PDebugPrint (4, "RD:%d/%d/%d/%d/%d\n", readAddress, wLength, stat,buffer[0], wLength );
							if (stat == CY_U3P_SUCCESS)
								CyU3PUsbSendEP0Data(wLength, buffer);
							else
								CyU3PUsbStall(0, CyTrue, CyFalse);

							CyU3PDebugPrint (4, "SOK\n" );
						}
						else
						{
							if( wLength == 0 )
							{
								CyU3PDebugPrint (4, "Rebooting\n" );

								// Host->Device (Reboot)
								CyU3PDeviceReset( CyFalse );

								// Code should not run.
								CyU3PUsbAckSetup();
							}
							//Write wIndex (LSW) ;;; wValue (MSW)
							uint32_t writeAddress = wIndex + (((uint32_t)wValue) << 16);
							uint8_t b512[512];
							while( wLength )
							{
								int towrite = wLength;
								if( towrite > 512 ) towrite = 512;
								memset( b512, 0xaa, 512 );
								uint16_t rc = 512;
								int r = CyU3PUsbGetEP0Data( towrite, b512, &rc );
								CyU3PDebugPrint (4, "C:%d/%d\n", rc, r );

								int page = 0;
								for( page = 0; page < 2; page++ )
								{
									preamble.length = 3;
									preamble.buffer[0] = 0xA0 | ((writeAddress>>15)&6);
									preamble.buffer[1] = ((writeAddress>>8)&0xff)+page; //address
									preamble.buffer[2] = writeAddress&0xff; //address
									preamble.ctrlMask = 0x0000;
									stat = CyU3PI2cTransmitBytes(&preamble, b512+page*256, 256, 0);
									if (stat != CY_U3P_SUCCESS)
										CyFxAppErrorHandler(stat);

									preamble.length = 1;
									stat |= CyU3PI2cWaitForAck(&preamble, 200);
									if (stat != CY_U3P_SUCCESS)
										CyFxAppErrorHandler(stat);

									CyU3PDebugPrint (4, "WR:%d/%d/%d/%d/%d\n", writeAddress, towrite, stat, b512[page*256], rc );
								}
								wLength -= towrite;
								writeAddress += towrite;
							}
							CyU3PUsbAckSetup();
						}
						break;
					default: /* unknown request, stall the endpoint. */
						CyU3PDebugPrint (4, "UVC/%d %d %d %d/\n", bReqType, bRequest, wLength, wValue );
						CyU3PUsbStall(0, CyTrue, CyFalse);
						break;
					}
				} else {
					CyU3PDebugPrint (4, "UNV/%d %d %d %d/\n", bReqType, bRequest, wLength, wValue );
					CyU3PUsbStall(0, CyTrue, CyFalse); /* Only vendor requests are expected. Stall anything else. */
				}
			}
		}
	}
}

/* Application define function which creates the threads. */
void CyFxApplicationDefine(void) {
	void *ptr = 0;
	uint32_t retThrdCreate = CY_U3P_SUCCESS;

	/* Create the event group required to defer vendor specific requests. */
	CyU3PEventCreate(&glAppEvent);
	/* Not expecting any failures here. So, not checking the return value. */

	/* Allocate the memory for the threads */
	ptr = CyU3PMemAlloc(CY_FX_ISOSRC_THREAD_STACK);

	/* Create the thread for the application */
	retThrdCreate = CyU3PThreadCreate(&isoSrcAppThread, /* ISO loop App Thread structure */
	"21:ISO_SRC_MANUAL__OUT", /* Thread ID and Thread name */
	IsoSrcAppThread_Entry, /* ISO loop App Thread Entry function */
	0, /* No input parameter to thread */
	ptr, /* Pointer to the allocated thread stack */
	CY_FX_ISOSRC_THREAD_STACK, /* ISO loop App Thread stack size */
	CY_FX_ISOSRC_THREAD_PRIORITY, /* ISO loop App Thread priority */
	CY_FX_ISOSRC_THREAD_PRIORITY, /* ISO loop App Thread priority */
	CYU3P_NO_TIME_SLICE, /* No time slice for the application thread */
	CYU3P_AUTO_START /* Start the Thread immediately */
	);

	/* Check the return code */
	if (retThrdCreate != 0) {
		/* Thread Creation failed with the error code retThrdCreate */

		/* Add custom recovery or debug actions here */

		/* Application cannot continue */
		/* Loop indefinitely */
		while (1)
			;
	}
}

/*
 * Main function
 */
int main(void) {
	CyU3PIoMatrixConfig_t io_cfg;
	CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

	///Confusing but seems to set it up so the SYS_CLK can be 400 MHz. from https://community.cypress.com/thread/21688
	CyU3PSysClockConfig_t clkCfg = {
			CyTrue, //If true, clock is 403.2 MHz, False, 384 MHz.
			2, 2, 2,
			CyFalse, CY_U3P_SYS_CLK };
	status = CyU3PDeviceInit(&clkCfg);
	if (status != CY_U3P_SUCCESS) {
		goto handle_fatal_error;
	}

	/* Initialize the caches. Enable both Instruction and Data Caches. */
	status = CyU3PDeviceCacheControl(CyTrue, CyTrue, CyTrue);
	if (status != CY_U3P_SUCCESS) {
		goto handle_fatal_error;
	}

	/* Configure the IO matrix for the device. On the FX3 DVK board, the COM port
	 * is connected to the IO(53:56). This means that either DQ32 mode should be
	 * selected or lppMode should be set to UART_ONLY. Here we are choosing
	 * UART_ONLY configuration. */
	io_cfg.isDQ32Bit = CyFalse;
	io_cfg.s0Mode = CY_U3P_SPORT_INACTIVE;
	io_cfg.s1Mode = CY_U3P_SPORT_INACTIVE;
	io_cfg.useUart = CyTrue;
	io_cfg.useI2C = CyTrue;
	io_cfg.useI2S = CyFalse;
	io_cfg.useSpi = CyFalse;
	io_cfg.lppMode = CY_U3P_IO_MATRIX_LPP_UART_ONLY;

	/* No GPIOs are enabled. */
	io_cfg.gpioSimpleEn[0] = 0;
	io_cfg.gpioSimpleEn[1] = 0;
	io_cfg.gpioComplexEn[0] = 0;
	io_cfg.gpioComplexEn[1] = 0;
	status = CyU3PDeviceConfigureIOMatrix(&io_cfg);
	if (status != CY_U3P_SUCCESS) {
		goto handle_fatal_error;
	}

	/* This is a non returnable call for initializing the RTOS kernel */
	CyU3PKernelEntry();

	CyU3PDebugPrint (4, "Device Init\r\n" );

	/* Dummy return to make the compiler happy */
	return 0;

	handle_fatal_error:

	CyU3PDebugPrint (4, "Fatal Start Error\r\n" );

	/* Cannot recover from this error. */
	while (1)
		;
}

/* [ ] */

