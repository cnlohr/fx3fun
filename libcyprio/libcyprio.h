/*
	Cypress FX3 library for easier C access directly to FX3 boards (mostly via CyUSB3.sys)
	
	(C) 2017 C. Lohr, under the MIT-x11 or NewBSD License.  You decide.
	*/

#ifndef CYPRIO_H
#define CYPRIO_H

#include <stdint.h>
#include "cyprio_aux.h"
#include "cyprio_usb100.h"

//Portions of this code are based on CyAPI's internal codebase. (Specifically in enumeration)
// I intend to rewrite that soon.  Also, I intend to clean up support for streaming isochronous out as well.
// Other people can feel free to contribute!!!

#ifndef BOOL
#define BOOL char
#endif

#define USB_STRING_MAXLEN 256
#define MAX_INTERFACES 8
#define MAX_ENDPOINTS 8
#define MAX_CONFIG_DESCRIPTORS 2

struct CyprIO;

struct CyprIOEndpoint	//Mimicing CCyUSBEndPoint
{
	struct CyprIO * parent;
	int assoc_iface;
	
	int is_superspeed;
	
    /* The fields of an EndPoint Descriptor */
    uint8_t   DscLen;
    uint8_t   DscType;
    uint8_t   Address;
    uint8_t   Attributes;
    uint16_t  MaxPktSize;
    uint16_t  PktsPerFrame;
    uint8_t   Interval;
    /* This are the fields for Super speed endpoint */
    uint8_t   ssdscLen;
    uint8_t   ssdscType;
    uint8_t   ssmaxburst;     /* Maximum number of packets endpoint can send in one burst */
    uint8_t   ssbmAttribute;  /* store endpoint attribute like for bulk it will be number of streams */
    uint16_t  ssbytesperinterval;

    BOOL    bIn;
};

struct CyprCyIsoPktInfo {
    int32_t Status;
    int32_t Length;
};


struct CyprIO
{
	USB_DEVICE_DESCRIPTOR USBDeviceDescriptor;
	int BytesXferedLastControl;
	int LastError;
	int StrLangID;
	WCHAR Manufacturer[USB_STRING_MAXLEN];
	WCHAR Product[USB_STRING_MAXLEN];
	WCHAR SerialNumber[USB_STRING_MAXLEN];
	PUSB_BOS_DESCRIPTOR pUsbBosDescriptor;
	PUSB_CONFIGURATION_DESCRIPTOR   USBConfigDescriptors[MAX_CONFIG_DESCRIPTORS];	//Must check and free.
	PUSB_INTERFACE_DESCRIPTOR		USBIfaceDescriptors[MAX_INTERFACES];
	int								USBIfaceAltSettings[MAX_INTERFACES];
	struct CyprIOEndpoint			CypIOEndpoints[MAX_ENDPOINTS];
	int								CyprIONumEps;
	
	int Interfaces, AltInterfaces;
	int SelInterface;
	
	uint8_t is_usb_3;

#if defined(WINDOWS) || defined( WIN32 )
	HANDLE hDevice;
#else
	struct libusb_device_handle * hDevice;
#endif
};



//Setup
int CyprIOConnect( struct CyprIO * ths, int index, int vid, int pid);
int CyprIOGetDevDescriptorInformation( struct CyprIO * ths );
int CyprIOSetup( struct CyprIO * ths, int use_config, int use_iface );
void CyprIODestroy( struct CyprIO * ths );

//Sort of utiltiy that binds the above 2. Mimics libusb_control_transfer from libusb, to ease portability.  Only for IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER.  Other messages must be done using the other mechanisms.  This however, can only make regular control message calls.
int CyprIOControlTransfer( struct CyprIO * ths, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char * data, uint16_t wLength, unsigned int timeout );


//Set up a high performance asynchronous transfer in from an ISO endpoint.
int CyprIODoCircularDataXferTx( struct CyprIOEndpoint * ep, int buffersize, int nrbuffers,  int (*callback)( void *, struct CyprIOEndpoint *, uint8_t *, uint32_t ), void * id );

int CyprIOGetString( struct CyprIO * ths, WCHAR *str, uint8_t sIndex);



///////////////////////////////////////////////////////////////////////
// CyprIO Utilities

//Tries to see if it can find a bootloader device, then flash the specified firmware.
int CyprIOBootloaderImage( const char * fwfile );

#endif
