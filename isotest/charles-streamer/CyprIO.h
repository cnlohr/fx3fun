#ifndef CYPRIO_H
#define CYPRIO_H

#include "devaux_header.h"
#include "usb100.h"
#include <windows.h>

//Portions of this code are based on CyAPI's internal codebase.
//  https://raw.githubusercontent.com/Cornell-RPAL/occam/master/indigosdk-2.0.15/third/cyusb/CyAPI.cpp

static GUID DrvGuid = {0xae18aa60, 0x7f6a, 0x11d4, 0x97, 0xdd, 0x0, 0x1, 0x2, 0x29, 0xb9, 0x59};

#define USB_STRING_MAXLEN 256
#define MAX_INTERFACES 8
#define MAX_ENDPOINTS 8

struct CyprIO;

typedef enum {XMODE_BUFFERED, XMODE_DIRECT } XFER_MODE_TYPE; //XXX TODO: Make this go away.

struct CyprIOEndpoint	//Mimicing CCyUSBEndPoint
{
	struct CyprIO * parent;
	int assoc_iface;
	
	int is_superspeed;
	
    /* The fields of an EndPoint Descriptor */
    UCHAR   DscLen;
    UCHAR   DscType;
    UCHAR   Address;
    UCHAR   Attributes;
    USHORT  MaxPktSize;
    USHORT  PktsPerFrame;
    UCHAR   Interval;
    /* This are the fields for Super speed endpoint */
    UCHAR   ssdscLen;
    UCHAR   ssdscType;
    UCHAR   ssmaxburst;     /* Maximum number of packets endpoint can send in one burst */
    UCHAR   ssbmAttribute;  /* store endpoint attribute like for bulk it will be number of streams */
    USHORT  ssbytesperinterval;

    /* Other fields */
//    ULONG   TimeOut;
//    ULONG   UsbdStatus;	//Unused
//    ULONG   NtStatus;	//Unused

//    DWORD   bytesWritten;
//    DWORD   LastError;
    BOOL    bIn;

    XFER_MODE_TYPE  XferMode;
};

struct CyprCyIsoPktInfo {
    LONG Status;
    LONG Length;
};


struct CyprIO
{
	HANDLE hDevice;
	USB_DEVICE_DESCRIPTOR USBDeviceDescriptor;
	int BytesXferedLastControl;
	int LastError;
	int StrLangID;
	wchar_t Manufacturer[USB_STRING_MAXLEN];
	wchar_t Product[USB_STRING_MAXLEN];
	wchar_t SerialNumber[USB_STRING_MAXLEN];
	PUSB_BOS_DESCRIPTOR pUsbBosDescriptor;
	char FriendlyName[USB_STRING_MAXLEN];
    UCHAR       USBAddress;
	PUSB_CONFIGURATION_DESCRIPTOR   USBConfigDescriptors[2];	//Must check and free.
	PUSB_INTERFACE_DESCRIPTOR		USBIfaceDescriptors[MAX_INTERFACES];
	int								USBIfaceAltSettings[MAX_INTERFACES];
	struct CyprIOEndpoint			CypIOEndpoints[MAX_ENDPOINTS];
	int								CyprIONumEps;
	
	int Interfaces, AltInterfaces;
	int SelInterface;
};



int CyprDataXfer( struct CyprIOEndpoint * ep, uint8_t * buf, uint32_t * bufLen, struct CyprCyIsoPktInfo* pktInfos);

//Setup
int CyprIOConnect( struct CyprIO * ths, int index, const char * matching );
int CyprIOGetDevDescriptorInformation( struct CyprIO * ths );
int CyprIOSetup( struct CyprIO * ths, int use_config, int use_iface );

//Control messages
int CyprIoControl(struct CyprIO * ths, ULONG cmd, uint8_t * XferBuf, ULONG len);
PSINGLE_TRANSFER FillSingleControlTransfer( char * buf, int bRequest, int wvhi, int wvlo, int wIndex, int wLength);
int CyprIOGetString( struct CyprIO * ths, wchar_t *str, UCHAR sIndex);


#endif