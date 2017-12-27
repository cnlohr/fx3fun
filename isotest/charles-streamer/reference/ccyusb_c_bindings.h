#ifndef _CCYUSB_C_BINDINGS
#define _CCYUSB_C_BINDINGS

//////////////////////////////////////////////////////////////////////
///////////   CORE BINDINGS FROM CyAPI.h   ///////////////////////////
//////////////////////////////////////////////////////////////////////

#include "reference/cyusb30_def.h"

/* Data straucture for the Vendor request and data */
typedef struct vendorCmdData
{
    UCHAR   *buf;       /* Pointer to the data */
    UCHAR   opCode;     /* Vendor request code */
    UINT    addr;       /* Read/Write address */
    long    size;       /* Size of the read/write */
    bool    isRead;     /* Read or write */
} vendorCmdData ;

#ifndef   __USB200_H__
#define   __USB200_H__
#pragma pack(push,1)
typedef struct _USB_DEVICE_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    USHORT  bcdUSB;
    UCHAR   bDeviceClass;
    UCHAR   bDeviceSubClass;
    UCHAR   bDeviceProtocol;
    UCHAR   bMaxPacketSize0;
    USHORT  idVendor;
    USHORT  idProduct;
    USHORT  bcdDevice;
    UCHAR   iManufacturer;
    UCHAR   iProduct;
    UCHAR   iSerialNumber;
    UCHAR   bNumConfigurations;
} USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;

typedef struct _USB_ENDPOINT_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    UCHAR   bEndpointAddress;
    UCHAR   bmAttributes;
    USHORT  wMaxPacketSize;
    UCHAR   bInterval;
} USB_ENDPOINT_DESCRIPTOR, *PUSB_ENDPOINT_DESCRIPTOR;

typedef struct _USB_CONFIGURATION_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    USHORT  wTotalLength;
    UCHAR   bNumInterfaces;
    UCHAR   bConfigurationValue;
    UCHAR   iConfiguration;
    UCHAR   bmAttributes;
    UCHAR   MaxPower;
} USB_CONFIGURATION_DESCRIPTOR, *PUSB_CONFIGURATION_DESCRIPTOR;

typedef struct _USB_INTERFACE_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    UCHAR   bInterfaceNumber;
    UCHAR   bAlternateSetting;
    UCHAR   bNumEndpoints;
    UCHAR   bInterfaceClass;
    UCHAR   bInterfaceSubClass;
    UCHAR   bInterfaceProtocol;
    UCHAR   iInterface;
} USB_INTERFACE_DESCRIPTOR, *PUSB_INTERFACE_DESCRIPTOR;

typedef struct _USB_STRING_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
    WCHAR   bString[1];
} USB_STRING_DESCRIPTOR, *PUSB_STRING_DESCRIPTOR;

typedef struct _USB_COMMON_DESCRIPTOR {
    UCHAR   bLength;
    UCHAR   bDescriptorType;
} USB_COMMON_DESCRIPTOR, *PUSB_COMMON_DESCRIPTOR;
#pragma pack(pop)
#endif

/*******************************************************************************/
class CCyIsoPktInfo {
public:
    LONG Status;
    LONG Length;
};

/*******************************************************************************/


/* {AE18AA60-7F6A-11d4-97DD-00010229B959} */
static GUID CYUSBDRV_GUID = {0xae18aa60, 0x7f6a, 0x11d4, 0x97, 0xdd, 0x0, 0x1, 0x2, 0x29, 0xb9, 0x59};

typedef enum {TGT_DEVICE, TGT_INTFC, TGT_ENDPT, TGT_OTHER } CTL_XFER_TGT_TYPE;
typedef enum {REQ_STD, REQ_CLASS, REQ_VENDOR } CTL_XFER_REQ_TYPE;
typedef enum {DIR_TO_DEVICE, DIR_FROM_DEVICE } CTL_XFER_DIR_TYPE;
typedef enum {XMODE_BUFFERED, XMODE_DIRECT } XFER_MODE_TYPE;

const int MAX_ENDPTS = 32;
const int MAX_INTERFACES = 255;
const int USB_STRING_MAXLEN = 256;

#define BUFSIZE_UPORT 2048 //4096 - CDT 130492
typedef enum { RAM = 1, I2CE2PROM, SPIFLASH } FX3_FWDWNLOAD_MEDIA_TYPE ;
typedef enum { SUCCESS = 0, FAILED, INVALID_MEDIA_TYPE, INVALID_FWSIGNATURE, DEVICE_CREATE_FAILED, INCORRECT_IMAGE_LENGTH, INVALID_FILE, SPILASH_ERASE_FAILED, CORRUPT_FIRMWARE_IMAGE_FILE,I2CE2PROM_UNKNOWN_I2C_SIZE } FX3_FWDWNLOAD_ERROR_CODE;




struct CCyUSBDevice
{
};

#endif

