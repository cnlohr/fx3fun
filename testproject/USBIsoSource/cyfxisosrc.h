/*
 ## Cypress USB 3.0 Platform header file (cyfxisosrc.h)
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

/* This file contains the constants used by the ISO source application example */

#ifndef _INCLUDED_CYFXISOSRC_H_
#define _INCLUDED_CYFXISOSRC_H_

#include "cyu3types.h"
#include "cyu3usbconst.h"
#include "cyu3externcstart.h"





//#define CY_FX_ISOSRC_DMA_TX_SIZE        (0)             /* DMA transfer size is set to infinite */
#define CY_FX_ISOSRC_THREAD_STACK       (0x1000)        /* Application thread stack size */
#define CY_FX_ISOSRC_THREAD_PRIORITY    (8)             /* Application thread priority */

/* Endpoint and socket definitions for the bulkloop application */
/* Note: For USB 2.0 the endpoints and corresponding sockets are one-to-one mapped
         i.e. EP 1 is mapped to UIB socket 1 and EP 2 to socket 2 so on */
#define CY_FX_EP_CONSUMER               0x83    /* EP 3 IN */
#define CY_FX_EP_CONSUMER_SOCKET        CY_U3P_UIB_SOCKET_CONS_3    /* Socket 3 is consumer */

/* Burst mode definitions: Only for super speed operation. The maximum burst mode 
 * supported is limited by the USB hosts available. The maximum value for this is 16
 * and the minimum (no-burst) is 1. */

#define CY_FX_ISOSRC_DMA_BUF_COUNT      (1)     /* Number of buffers in the DMA channel. (Now applied to GPIF) */
                                                /* NOTE: Count is per socket! */
#define CY_FX_ISO_PKTS                  (2)     /* Number of bursts per microframe. */
#define CY_FX_ISO_BURST                 (16)    /* Number of packets per burst. */
#define MAX_PACKET_SIZE                 1024
#define MULTI_CHANNEL_XFER_SIZE 16384


#define CY_FX_DEBUG_PRIORITY                    (4)             /* Sets the debug print priority level */

#define DMAMULTI //Needed for continuous operation (Should remove where this is false)

/* Extern definitions for the USB Descriptors */
extern const uint8_t CyFxUSB20DeviceDscr[];
extern const uint8_t CyFxUSB30DeviceDscr[];
extern const uint8_t CyFxUSBDeviceQualDscr[];
extern const uint8_t CyFxUSBFSConfigDscr[];
extern const uint8_t CyFxUSBHSConfigDscr[];
extern const uint8_t CyFxUSBBOSDscr[];
extern const uint8_t CyFxUSBSSConfigDscr[];
extern const uint8_t CyFxUSBStringLangIDDscr[];
extern const uint8_t CyFxUSBManufactureDscr[];
extern const uint8_t CyFxUSBProductDscr[];

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYFXISOSRC_H_ */

/*[]*/
