#ifndef _DEVAUX_HEADER
#define _DEVAUX_HEADER



	
#if defined( WINDOWS ) || defined( WIN32 )
#include <windows.h>
#else
#define UCHAR uint8_t
#define USHORT uint16_t
#define ULONG uint32_t
#define WCHAR uint16_t
#define ZeroMemory( mem, size ) memset( (mem), (0), (size) )
#endif


#if defined( WINDOWS ) || defined( WIN32 )
#include <windows.h>
#ifdef TCC
	typedef struct _SP_DEVINFO_DATA {
		DWORD     cbSize;
		GUID      ClassGuid;
		DWORD     DevInst;
		ULONG_PTR Reserved;
	} SP_DEVINFO_DATA, *PSP_DEVINFO_DATA;
	typedef struct _SP_DEVICE_INTERFACE_DATA {
		DWORD     cbSize;
		GUID      InterfaceClassGuid;
		DWORD     Flags;
		ULONG_PTR Reserved;
	} SP_DEVICE_INTERFACE_DATA, *PSP_DEVICE_INTERFACE_DATA;
	typedef struct _SP_DEVICE_INTERFACE_DETAIL_DATA {
		DWORD cbSize;
		CHAR DevicePath[ANYSIZE_ARRAY];
	} SP_DEVICE_INTERFACE_DETAIL_DATA_A, *PSP_DEVICE_INTERFACE_DETAIL_DATA_A;
	typedef PVOID HDEVINFO;
		
	HDEVINFO WINAPI SetupDiGetClassDevsA(CONST GUID*,PCSTR,HWND,DWORD);
	
	BOOL SetupDiGetDeviceInterfaceDetailA(
		HDEVINFO                         DeviceInfoSet,
		PSP_DEVICE_INTERFACE_DATA        DeviceInterfaceData,
		PSP_DEVICE_INTERFACE_DETAIL_DATA_A DeviceInterfaceDetailData,
		DWORD                            DeviceInterfaceDetailDataSize,
		PDWORD                           RequiredSize,
		PSP_DEVINFO_DATA                 DeviceInfoData
	);

	BOOL SetupDiEnumDeviceInterfaces(
		HDEVINFO                  DeviceInfoSet,
		PSP_DEVINFO_DATA          DeviceInfoData,
		const GUID                      *InterfaceClassGuid,
		DWORD                     MemberIndex,
		PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData );
		
	BOOL SetupDiDestroyDeviceInfoList( HDEVINFO DeviceInfoSet	);


	#define DIGCF_PRESENT	0x00000002
	#define DIGCF_INTERFACEDEVICE	0x00000010
	#define DIGCF_DEVICEINTERFACE	0x00000010
	#define SPDRP_CLASS	7
	#define SPDRP_DRIVER	9
	#define FILE_DEVICE_KEYBOARD            0x0000000b
	#define METHOD_OUT_DIRECT                 2
	enum
	{ FILE_ANY_ACCESS			= 0x00000000UL,
		FILE_SPECIAL_ACCESS			= FILE_ANY_ACCESS,
		FILE_READ_ACCESS			= 0x00000001UL,
		FILE_WRITE_ACCESS			= 0x00000002UL
	};
	#define CTL_CODE(t,f,m,a) (((t)<<16)|((a)<<14)|((f)<<2)|(m))

	
	#define FILE_DEVICE_UNKNOWN             0x00000022	
	
		
	BOOL WINAPI DeviceIoControl(
		HANDLE       hDevice,
		DWORD        dwIoControlCode,
		LPVOID       lpInBuffer,
		DWORD        nInBufferSize,
		LPVOID       lpOutBuffer,
		DWORD        nOutBufferSize,
		LPDWORD      lpBytesReturned,
		LPOVERLAPPED lpOverlapped
	);
	

	
	
	
	#pragma pack(1)
	
	
	
	//These are from cyioctl.h

	#define DIR_HOST_TO_DEVICE 0
	#define DIR_DEVICE_TO_HOST 1


	#define DEVICE_SPEED_UNKNOWN        0x00000000
	#define DEVICE_SPEED_LOW_FULL       0x00000001
	#define DEVICE_SPEED_HIGH           0x00000002
	#define DEVICE_SPEED_SUPER	0x00000004


	typedef struct _ISO_PACKET_INFO {
		ULONG Status;
		ULONG Length;
	} ISO_PACKET_INFO, *PISO_PACKET_INFO;

	typedef struct _WORD_SPLIT {
		UCHAR lowByte;
		UCHAR hiByte;
	} WORD_SPLIT, *PWORD_SPLIT;
	
	typedef struct _BM_REQ_TYPE {
		UCHAR   Recipient:2;
		UCHAR   Reserved:3;
		UCHAR   Type:2;
		UCHAR   Direction:1;
	} BM_REQ_TYPE, *PBM_REQ_TYPE;

	typedef struct _SETUP_PACKET {

		union {
			BM_REQ_TYPE bmReqType;
			UCHAR bmRequest;
		};

		UCHAR bRequest;

		union {
			WORD_SPLIT wVal;
			USHORT wValue;
		};

		union {
			WORD_SPLIT wIndx;
			USHORT wIndex;
		};

		union {
			WORD_SPLIT wLen;
			USHORT wLength;
		};

		ULONG ulTimeOut;

	} SETUP_PACKET, *PSETUP_PACKET;

		
	#define USB_ISO_ID                  0x4945
	#define USB_ISO_CMD_ASAP            0x8000
	#define USB_ISO_CMD_CURRENT_FRAME   0x8001
	#define USB_ISO_CMD_SET_FRAME       0x8002

		
	typedef struct _ISO_ADV_PARAMS {

		USHORT isoId;
		USHORT isoCmd;

		ULONG ulParam1;
		ULONG ulParam2;

	} ISO_ADV_PARAMS, *PISO_ADV_PARAMS;

	typedef struct _SINGLE_TRANSFER {
		union {
			SETUP_PACKET SetupPacket;
			ISO_ADV_PARAMS IsoParams;
		};

		UCHAR reserved;

		UCHAR ucEndpointAddress;
		ULONG NtStatus;
		ULONG UsbdStatus;
		ULONG IsoPacketOffset;
		ULONG IsoPacketLength;
		ULONG BufferOffset;
		ULONG BufferLength;
	} SINGLE_TRANSFER, *PSINGLE_TRANSFER;

		
	#define METHOD_BUFFERED	0
	#define METHOD_IN_DIRECT	1
	#define METHOD_OUT_DIRECT	2
	#define METHOD_NEITHER	    3

	#define IOCTL_ADAPT_INDEX 0x0000

	// Get the driver version
	#define IOCTL_ADAPT_GET_DRIVER_VERSION         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Get the current USBDI version
	#define IOCTL_ADAPT_GET_USBDI_VERSION         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+1, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Get the current device alt interface settings from driver
	#define IOCTL_ADAPT_GET_ALT_INTERFACE_SETTING CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+2, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Set the device interface and alt interface setting
	#define IOCTL_ADAPT_SELECT_INTERFACE          CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+3, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Get device address from driver
	#define IOCTL_ADAPT_GET_ADDRESS               CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+4, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Get number of endpoints for current interface and alt interface setting from driver
	#define IOCTL_ADAPT_GET_NUMBER_ENDPOINTS      CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+5, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Get the current device power state
	#define IOCTL_ADAPT_GET_DEVICE_POWER_STATE    CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+6,   METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Set the device power state
	#define IOCTL_ADAPT_SET_DEVICE_POWER_STATE    CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+7,   METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Send a raw packet to endpoint 0
	#define IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+8, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Send/receive data to/from nonep0
	#define IOCTL_ADAPT_SEND_NON_EP0_TRANSFER     CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+9, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Simulate a disconnect/reconnect
	#define IOCTL_ADAPT_CYCLE_PORT                CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+10, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Reset the pipe
	#define IOCTL_ADAPT_RESET_PIPE                CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+11, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Reset the device
	#define IOCTL_ADAPT_RESET_PARENT_PORT         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+12, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Get the current transfer size of an endpoint (in number of bytes)
	#define IOCTL_ADAPT_GET_TRANSFER_SIZE         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+13, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Set the transfer size of an endpoint (in number of bytes)
	#define IOCTL_ADAPT_SET_TRANSFER_SIZE         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+14, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Return the name of the device
	#define IOCTL_ADAPT_GET_DEVICE_NAME           CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+15, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Return the "Friendly Name" of the device
	#define IOCTL_ADAPT_GET_FRIENDLY_NAME         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+16, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Abort all outstanding transfers on the pipe
	#define IOCTL_ADAPT_ABORT_PIPE                CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+17, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Send/receive data to/from nonep0 w/ direct buffer acccess (no buffering)
	#define IOCTL_ADAPT_SEND_NON_EP0_DIRECT       CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+18, METHOD_NEITHER, FILE_ANY_ACCESS)

	// Return device speed
	#define IOCTL_ADAPT_GET_DEVICE_SPEED          CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+19, METHOD_BUFFERED, FILE_ANY_ACCESS)

	// Get the current USB frame number
	#define IOCTL_ADAPT_GET_CURRENT_FRAME         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_ADAPT_INDEX+20, METHOD_BUFFERED, FILE_ANY_ACCESS)

	#define NUMBER_OF_ADAPT_IOCTLS 21 // Last IOCTL_ADAPT_INDEX + 1
#else
	#include <setupapi.h>
	#include <winioctl.h>
#endif

#else
	#include <libusb-1.0/libusb.h>
#endif



	

//These are from cyusb30_def.hDevice
// USB3.0 specific constant defination
#define BCDUSBJJMASK  0xFF00 //(0xJJMN JJ - Major version,M Minor version, N sub-minor vesion)
#define USB30MAJORVER 0x0300
#define USB20MAJORVER 0x0200

#define USB_BOS_DESCRIPTOR_TYPE			      0x0F
#define USB_DEVICE_CAPABILITY                 0x10
#define USB_SUPERSPEED_ENDPOINT_COMPANION     0x30
#define USB_BOS_CAPABILITY_TYPE_Wireless_USB  0x01
#define USB_BOS_CAPABILITY_TYPE_USB20_EXT	  0x02
#define USB_BOS_CAPABILITY_TYPE_SUPERSPEED_USB    0x03
#define USB_BOS_CAPABILITY_TYPE_CONTAINER_ID       0x04
#define USB_BOS_CAPABILITY_TYPE_CONTAINER_ID_SIZE  0x10

#define USB_BOS_DEVICE_CAPABILITY_TYPE_INDEX 0x2
//constant defination
typedef struct _USB_BOS_DESCRIPTOR
{
	UCHAR bLength;/* Descriptor length*/
	UCHAR bDescriptorType;/* Descriptor Type */
	USHORT wTotalLength;/* Total length of descriptor ( icluding device capability*/
	UCHAR bNumDeviceCaps;/* Number of device capability descriptors in BOS  */
}USB_BOS_DESCRIPTOR,*PUSB_BOS_DESCRIPTOR;



typedef struct _USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR
{
	UCHAR bLength;
	UCHAR bDescriptorType;
	UCHAR bMaxBurst;
	UCHAR bmAttributes;        
	USHORT bBytesPerInterval;
}USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR,*PUSB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR;



#endif
