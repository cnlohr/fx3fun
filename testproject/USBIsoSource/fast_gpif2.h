/*
 ## Cypress FX3 Firmware Example Header File (cyfxsrammaster.h)
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

/* This file contains the constants and definitions used by the FX3-SRAM Master application example */

#ifndef _INCLUDED_FAST_GPIF2_H_
#define _INCLUDED_FAST_GPIF2_H_

#include "cyu3types.h"
#include "cyu3usbconst.h"
#include "cyu3externcstart.h"

#define DMA_IN_BUF_SIZE                         (1024)  /* Size of DMA buffer used for SRAM to USB transfer. */
#define CY_FX_SRAM_DMA_BUF_COUNT_P_2_U          (3)     /* Number of DMA buffers used for SRAM to USB transfer. */
#define CY_FX_PRODUCER_PPORT_SOCKET             (CY_U3P_PIB_SOCKET_0)           /* GPIF Socket 0 is producer. */

/* Endpoint and socket definitions for the application */

/* To change the Producer and Consumer EP enter the appropriate EP numbers for the #defines.
 * In the case of IN endpoints enter EP number along with the direction bit.
 * For eg. EP 6 IN endpoint is 0x86
 *     and EP 6 OUT endpoint is 0x06.
 * To change sockets mention the appropriate socket number in the #defines. */

/* Note: For USB 2.0 the endpoints and corresponding sockets are one-to-one mapped
         i.e. EP 1 is mapped to UIB socket 1 and EP 2 to socket 2 so on */

//#define CY_FX_EP_PRODUCER                       (0x01)  /* Endpoint 1-OUT. */
//#define CY_FX_EP_CONSUMER                       (0x81)  /* Endpoint 1-IN. */
//#define CY_FX_PRODUCER_USB_SOCKET               (CY_U3P_UIB_SOCKET_PROD_1)      /* USB Socket 1 is producer */
//#define CY_FX_CONSUMER_USB_SOCKET               (CY_U3P_UIB_SOCKET_CONS_1)      /* USB Socket 1 is consumer */

//#define CY_FX_CONSUMER_PPORT_SOCKET             (CY_U3P_PIB_SOCKET_3)           /* GPIF Socket 3 is consumer. */

#define LED_RATE_CHANGE_COMMAND                 (0xAA)          /* Vendor command used to change LED blink rate. */
#define SRAM_READ_COMMAND                       (0xBB)          /* Vendor command for SRAM read. */
#define DEVICE_RESET_COMMAND                    (0xE0)          /* Vendor command for device reset. */

#define BURST_LEN                               (16)             /* Burst is set to 1. */
#define LED_GPIO                                (54)            /* GPIO used for transfer indication. */
#define LED_BLINK_RATE_SS                       (50)            /* LED blink rate (in ms) for SuperSpeed */

#define CY_FX_FULL_SPEED_EP_SIZE                (64)            /* Bulk endpoint size for USB Full Speed. */
#define CY_FX_HIGH_SPEED_EP_SIZE                (512)           /* Bulk endpoint size for USB Hi-Speed. */
#define CY_FX_SUPER_SPEED_EP_SIZE               (1024)          /* Bulk endpoint size for USB SuperSpeed. */

#define CY_FX_DEBUG_PRIORITY                    (4)             /* Sets the debug print priority level */

#include "cyu3externcend.h"

#endif /* _INCLUDED_CYFXSRAMMASTER_H_ */

/*[]*/
