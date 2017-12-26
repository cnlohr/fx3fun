#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "devaux_header.h"
#include "usb100.h"

//Portions of this code are based on CyAPI's internal codebase.
//  https://raw.githubusercontent.com/Cornell-RPAL/occam/master/indigosdk-2.0.15/third/cyusb/CyAPI.cpp

static GUID DrvGuid = {0xae18aa60, 0x7f6a, 0x11d4, 0x97, 0xdd, 0x0, 0x1, 0x2, 0x29, 0xb9, 0x59};

#define USB_STRING_MAXLEN 256

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
};


#define DEBUGINFO(x...) fprintf( stderr, x );

int CyprIOConnect( struct CyprIO * ths, int index, const char * matching )
{
	int Devices = 0;

    //SP_DEVINFO_DATA devInfoData;
    SP_DEVICE_INTERFACE_DATA  devInterfaceData;

    //Open a handle to the plug and play dev node.
    //SetupDiGetClassDevs() returns a device information set that contains info on all
    // installed devices of a specified class which are present.
    HDEVINFO hwDeviceInfo = SetupDiGetClassDevsA ( (LPGUID) &DrvGuid,
        NULL,
        NULL,
        DIGCF_PRESENT|DIGCF_INTERFACEDEVICE);

    Devices = 0;

    if (hwDeviceInfo != INVALID_HANDLE_VALUE) {
        //SetupDiEnumDeviceInterfaces() returns information about device interfaces
        // exposed by one or more devices. Each call returns information about one interface.
        //The routine can be called repeatedly to get information about several interfaces
        // exposed by one or more devices.
        devInterfaceData.cbSize = sizeof(devInterfaceData);

        // Iterate the number of devices
        int iDevice =0;
        Devices = 0;
        int bDone = 0;

        while (!bDone) {
			//SetupDiEnumDeviceInterfaces() returns information about device interfaces
			// exposed by one or more devices. Each call returns information about one interface.
			//The routine can be called repeatedly to get information about several interfaces
			// exposed by one or more devices.
			
			devInterfaceData.cbSize = sizeof(devInterfaceData);
			
            BOOL bRetVal = SetupDiEnumDeviceInterfaces (hwDeviceInfo, 0, (LPGUID) &DrvGuid, iDevice, &devInterfaceData);
			//Make sure we get a valid handle first.			
			if( !bRetVal )
			{
                ths->LastError = GetLastError();
                if (ths->LastError == ERROR_NO_MORE_ITEMS)
					bDone = TRUE;
				else
					DEBUGINFO( "Error: Got confusing error from SetupDiEnumDeviceInterfaces (%d)\n", ths->LastError );
				iDevice++;
				continue;
			}
			
			//Allocate a function class device data structure to receive the goods about this
			// particular device.
			ULONG requiredLength = 0;
			SetupDiGetDeviceInterfaceDetailA ( hwDeviceInfo, &devInterfaceData, NULL, 0, &requiredLength, NULL);

			ULONG predictedLength = requiredLength;

			SP_DEVICE_INTERFACE_DETAIL_DATA_A * functionClassDeviceData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A) malloc (predictedLength);
			functionClassDeviceData->cbSize =  sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA_A);
			SP_DEVINFO_DATA devInfoData;
			
			devInfoData.cbSize = sizeof(devInfoData);
			
			//Retrieve the information from Plug and Play */
			bRetVal = SetupDiGetDeviceInterfaceDetailA (hwDeviceInfo,
				&devInterfaceData,
				functionClassDeviceData,
				predictedLength,
				&requiredLength,
				&devInfoData);
			if( !bRetVal )
			{
				DEBUGINFO( "Error: SetupDiGetDeviceInterfaceDetail failed\n" );
				iDevice++;
				continue;
			}

			//Now, we can trull through the data returned to us.
			CHAR        DevPath[USB_STRING_MAXLEN];
			SP_DEVICE_INTERFACE_DETAIL_DATA_A tmpInterfaceDeviceDetailData;

			/* NOTE : x64 packing issue ,requiredLength return 5byte size of the (SP_INTERFACE_DEVICE_DETAIL_DATA) and functionClassDeviceData needed sizeof functionClassDeviceData 8byte */
			int pathLen = requiredLength - (sizeof (tmpInterfaceDeviceDetailData.cbSize)+sizeof (tmpInterfaceDeviceDetailData.DevicePath));
			
			memcpy (DevPath, functionClassDeviceData->DevicePath, pathLen);
			DevPath[pathLen] = 0;
			
			if( index < 0 || matching == 0 )
			{
				printf( "Found DP %d: %s\n", iDevice, DevPath );
				iDevice++;
				continue;
			}
			printf( "%s %s %d\n", DevPath, matching, index );
			if( strcmp( DevPath, matching ) != 0 || index-- )
			{
				//Next device.
				iDevice++;
				continue;
			}
			
			// THIS is the device we're after.
			HANDLE hFile = CreateFile (DevPath,
				GENERIC_WRITE | GENERIC_READ,
				FILE_SHARE_WRITE | FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,
				FILE_FLAG_OVERLAPPED,
				NULL );
				
			ths->LastError =  GetLastError();
			free(functionClassDeviceData);

			if (hFile == INVALID_HANDLE_VALUE)
			{
				DEBUGINFO( "Error: Could not open device name %s with error code %d\n", DevPath, ths->LastError );
				//Next device.
				iDevice++;
				continue;
			}
			
			ths->hDevice = hFile;
			Devices = 1;
			SetupDiDestroyDeviceInfoList(hwDeviceInfo);
			return 0;
        }

        SetupDiDestroyDeviceInfoList(hwDeviceInfo);
    }

    return -1;
}



PSINGLE_TRANSFER FillSingleControlTransfer( char * buf, int bRequest, int wvhi, int wvlo, int wIndex, int wLength)
{
	PSINGLE_TRANSFER pSingleTransfer = (PSINGLE_TRANSFER) buf;
    pSingleTransfer->SetupPacket.bmReqType.Direction = DIR_DEVICE_TO_HOST;
    pSingleTransfer->SetupPacket.bmReqType.Type      = 0;
    pSingleTransfer->SetupPacket.bmReqType.Recipient = 0;
    pSingleTransfer->SetupPacket.bRequest = bRequest;
    pSingleTransfer->SetupPacket.wVal.hiByte = wvhi;
    pSingleTransfer->SetupPacket.wVal.lowByte = wvlo;
    pSingleTransfer->SetupPacket.wLength = wLength;
    pSingleTransfer->SetupPacket.ulTimeOut = 5;
    pSingleTransfer->BufferLength = pSingleTransfer->SetupPacket.wLength;
    pSingleTransfer->BufferOffset = sizeof(SINGLE_TRANSFER);
	return pSingleTransfer;
}
	
	
	

int CyprIoControl(struct CyprIO * ths, ULONG cmd, PUCHAR XferBuf, ULONG len)
{
    if ( ths->hDevice == INVALID_HANDLE_VALUE ) return 0;
    BOOL bDioRetVal = DeviceIoControl (ths->hDevice, cmd, XferBuf, len, XferBuf, len, &ths->BytesXferedLastControl, NULL);
    if(!bDioRetVal) { ths->LastError = GetLastError(); ths->BytesXferedLastControl = -1; }
	return !bDioRetVal || (ths->BytesXferedLastControl != len);
}


int CyprIOGetString( struct CyprIO * ths, wchar_t *str, UCHAR sIndex)
{
    // Get the header to find-out the number of languages, size of lang ID list
    ULONG length = sizeof(SINGLE_TRANSFER) + sizeof(USB_COMMON_DESCRIPTOR);
    UCHAR buf[length];
	int bRetVal;
	ZeroMemory (str, USB_STRING_MAXLEN);
	ZeroMemory( buf, sizeof(buf ) );
	
    USB_COMMON_DESCRIPTOR cmnDescriptor;
	PSINGLE_TRANSFER pSingleTransfer;
	
	pSingleTransfer = FillSingleControlTransfer( buf,USB_REQUEST_GET_DESCRIPTOR, USB_STRING_DESCRIPTOR_TYPE, sIndex, ths->StrLangID, sizeof(USB_COMMON_DESCRIPTOR) );
	bRetVal = CyprIoControl( ths,IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    if (bRetVal)
	{
		DEBUGINFO( "Error getting length of string\n" );
		return bRetVal;
	}
	

	memcpy(&cmnDescriptor, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), sizeof(USB_COMMON_DESCRIPTOR));

	// Get the entire descriptor
	length = sizeof(SINGLE_TRANSFER) + cmnDescriptor.bLength;
	UCHAR buf2[length];
	ZeroMemory( buf2, sizeof(buf2 ) );
	pSingleTransfer = FillSingleControlTransfer( buf2, USB_REQUEST_GET_DESCRIPTOR, USB_STRING_DESCRIPTOR_TYPE, sIndex, ths->StrLangID, cmnDescriptor.bLength );
	bRetVal = CyprIoControl(ths, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf2, length);

	
	UCHAR bytes = (buf2[sizeof(SINGLE_TRANSFER)]);
	UCHAR signature = (buf2[sizeof(SINGLE_TRANSFER)+1]);
	
	if (!bRetVal && (bytes>2) && (signature == 0x03)) {
		memcpy(str, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset+2), bytes-2);
		return 0;
	}
	return 1;
}


static int CyprIOGetInteralBOSDescriptor( struct CyprIO * ths )
{
    ULONG length = sizeof(SINGLE_TRANSFER) + sizeof(USB_BOS_DESCRIPTOR);
    UCHAR buf[length];
	int bRetVal;
    PSINGLE_TRANSFER pSingleTransfer;

	pSingleTransfer = FillSingleControlTransfer( buf, USB_REQUEST_GET_DESCRIPTOR, USB_BOS_DESCRIPTOR_TYPE, 0, 0, sizeof(USB_BOS_DESCRIPTOR) );
	bRetVal = CyprIoControl(ths, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    if (bRetVal) return bRetVal;

	PUSB_BOS_DESCRIPTOR BosDesc = (PUSB_BOS_DESCRIPTOR)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset);
	USHORT BosDescLength= BosDesc->wTotalLength;

	length = sizeof(SINGLE_TRANSFER) + BosDescLength;
	UCHAR buf2[length];
	pSingleTransfer = FillSingleControlTransfer( buf2, USB_REQUEST_GET_DESCRIPTOR, USB_BOS_DESCRIPTOR_TYPE, 0, 0, BosDescLength );
	bRetVal = CyprIoControl(ths, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf2, length);
	if (bRetVal) return bRetVal;

	ths->pUsbBosDescriptor = (PUSB_BOS_DESCRIPTOR)malloc(BosDescLength);        
	memcpy(ths->pUsbBosDescriptor, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), BosDescLength);
    return bRetVal;
}



static int PrintWString( wchar_t * ct )
{
	int i;
	for( i = 0; ct[i]; i++ )
	{
		printf( "%c", ct[i] );
	}		
}

//This function really should only be called internally, by CyprIOSetup.
static int CyprIOGetDevDescriptor( struct CyprIO * ths )
{
	PSINGLE_TRANSFER pSingleTransfer;
	HANDLE hd = ths->hDevice;
	char buf[80];
	int bRetVal;
    ULONG length;
	USB_COMMON_DESCRIPTOR cmnDescriptor;

    //USB_DEVICE_DESCRIPTOR devDescriptor;
    pSingleTransfer = FillSingleControlTransfer( buf, USB_REQUEST_GET_DESCRIPTOR, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, sizeof(USB_DEVICE_DESCRIPTOR) );
	length = sizeof(SINGLE_TRANSFER) + sizeof(USB_DEVICE_DESCRIPTOR);
	bRetVal = CyprIoControl( ths, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    if (bRetVal) {	return bRetVal;	}
	memcpy(&ths->USBDeviceDescriptor, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), sizeof(USB_DEVICE_DESCRIPTOR));

	//printf( "Got USB Descriptor for: %04x:%04x\n", ths->USBDeviceDescriptor.idVendor, ths->USBDeviceDescriptor.idProduct );

    pSingleTransfer = FillSingleControlTransfer( buf, USB_REQUEST_GET_DESCRIPTOR, USB_STRING_DESCRIPTOR_TYPE, 0, 0, sizeof(USB_COMMON_DESCRIPTOR) );
	length = sizeof(SINGLE_TRANSFER) + sizeof(USB_COMMON_DESCRIPTOR);
	bRetVal = CyprIoControl( ths, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
	memcpy(&cmnDescriptor, (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), sizeof(USB_COMMON_DESCRIPTOR));
	if( bRetVal ) { return bRetVal; }
	
	int LangIDs = (cmnDescriptor.bLength - 2 ) / 2;
	// Get the entire descriptor, all LangIDs
    pSingleTransfer = FillSingleControlTransfer( buf, USB_REQUEST_GET_DESCRIPTOR, USB_STRING_DESCRIPTOR_TYPE, 0, 0, cmnDescriptor.bLength );
	length = sizeof(SINGLE_TRANSFER) + cmnDescriptor.bLength;
	bRetVal = CyprIoControl( ths, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
	if( bRetVal ) { return bRetVal; }
	PUSB_STRING_DESCRIPTOR IDs = (PUSB_STRING_DESCRIPTOR) (buf + sizeof(SINGLE_TRANSFER));

	ths->StrLangID =(LangIDs) ? IDs[0].bString[0] : 0;

	for (int i=0; i<LangIDs; i++) {
		USHORT id = IDs[i].bString[0];
		if (id == 0x0409) ths->StrLangID = id;
	}

	CyprIOGetString(ths,ths->Manufacturer,ths->USBDeviceDescriptor.iManufacturer);
	CyprIOGetString(ths,ths->Product,ths->USBDeviceDescriptor.iProduct);
	CyprIOGetString(ths,ths->SerialNumber,ths->USBDeviceDescriptor.iSerialNumber);
	printf( ":" ); PrintWString( ths->Manufacturer );
	printf( ":" ); PrintWString( ths->Product );
	printf( ":" ); PrintWString( ths->SerialNumber );
	printf( "\n" ); 
	
	
	if ((ths->USBDeviceDescriptor.bcdUSB & BCDUSBJJMASK) != USB30MAJORVER)
	{
		DEBUGINFO( "Error: BcdUSB indicates this is not a USB 3.0 compliant device.  Not supported with this client.\n" );
		return 1;
	}
	
	if(CyprIOGetInteralBOSDescriptor( ths ) )
	{
		DEBUGINFO( "Error: Failed to get Internal BOS Descriptor\n" );
		return 1;
	}
	
	//Theoretically, we should be processing the BOS descriptor here...  I don't know if we acutally need to.
	if( CyprIoControl( ths, IOCTL_ADAPT_GET_ADDRESS, &ths->USBAddress, 1L) )
	{
		DEBUGINFO ("Failed to get USB address\n" );
		return 1;
	}

    ZeroMemory(ths->FriendlyName, USB_STRING_MAXLEN);
	CyprIoControl(ths, IOCTL_ADAPT_GET_FRIENDLY_NAME, (PUCHAR)ths->FriendlyName, USB_STRING_MAXLEN);
	if( ths->BytesXferedLastControl <= 0 )
	{
		DEBUGINFO( "Failed to get friendly name %d\n", ths->LastError );
		return 1;
	}
	//You can also get IOCTL_ADAPT_GET_DEVICE_NAME the same way but I don't care for it.
	//Also bool bRetVal = IoControl(IOCTL_ADAPT_GET_DRIVER_VERSION, (PUCHAR) &DriverVersion, sizeof(ULONG)); for Driver version.
	//Also bool bRetVal = IoControl(IOCTL_ADAPT_GET_USBDI_VERSION, (PUCHAR) &USBDIVersion, sizeof(ULONG));

	
	ULONG speed = 0;
	int bHighSpeed = 0;
	int bSuperSpeed = 0;
	CyprIoControl( ths, IOCTL_ADAPT_GET_DEVICE_SPEED, (PUCHAR)&speed, sizeof(speed));
	bHighSpeed = (speed == DEVICE_SPEED_HIGH);
	bSuperSpeed = (speed == DEVICE_SPEED_SUPER);
	if( !bSuperSpeed )
	{
		DEBUGINFO( "Error: Super speed bit not set!\n" );
		return -3;
	}

	//XXX TODO !!! PICK UP HERE... In the cypress code, it's at:
	
/*
        // Gets the config (including interface and endpoint) descriptors from the device
        for (int i=0; i<Configs; i++) 
		{
            GetCfgDescriptor(i);
*/
	
	
	printf( "Successfully connected to: %s\n", ths->FriendlyName );
	return 0;
}


int CyprIOSetup( struct CyprIO * ths )
{
	HANDLE hd = ths->hDevice;
	if( !hd )
	{
		DEBUGINFO( "Error: CyprIOSetup called on a non-open device\n" );
		return -1;
	}
	if( CyprIOGetDevDescriptor( ths ) )
	{
		DEBUGINFO( "Couldn't get device descriptor\n" );
		return -2;
	}
	
	return 0;
}

#if 0

void GetStreamerDevice()
{
	USBDevice = new CCyUSBDevice((HANDLE)this->Handle,CYUSBDRV_GUID,true);

	if (USBDevice == NULL) return;

	/////////////////////////////////////////////////////////////////
	// Walk through all devices looking for VENDOR_ID/PRODUCT_ID
	// We No longer got restricted with vendor ID and Product ID.
	// Check for vendor ID and product ID is discontinued.
	///////////////////////////////////////////////////////////////////
	for (int i=0; i<n; i++)
	{
		//if ((USBDevice->VendorID == VENDOR_ID) && (USBDevice->ProductID == PRODUCT_ID)) 
		//    break;
		USBDevice->Open(i);
		String *strDeviceData = "";
		strDeviceData = String::Concat(strDeviceData, "(0x");
		strDeviceData = String::Concat(strDeviceData, USBDevice->VendorID.ToString("X4"));
		strDeviceData = String::Concat(strDeviceData, " - 0x");
		strDeviceData = String::Concat(strDeviceData, USBDevice->ProductID.ToString("X4"));
		strDeviceData = String::Concat(strDeviceData, ") ");
		strDeviceData = String::Concat(strDeviceData, USBDevice->FriendlyName);               
		
		DeviceComboBox->Items->Add(strDeviceData);      
		DeviceComboBox->Enabled = true;
	}
	if (n > 0 ) {
		DeviceComboBox->SelectedIndex = 0;
		USBDevice->Open(0);
	}


	//if ((USBDevice->VendorID == VENDOR_ID) && (USBDevice->ProductID == PRODUCT_ID)) 
	{
		StartBtn->Enabled = true;

		int interfaces = USBDevice->AltIntfcCount()+1;

		bHighSpeedDevice = USBDevice->bHighSpeed;
		bSuperSpeedDevice = USBDevice->bSuperSpeed;

		for (int i=0; i< interfaces; i++)
		{
			if (USBDevice->SetAltIntfc(i) == true )
			{

				int eptCnt = USBDevice->EndPointCount();

				// Fill the EndPointsBox
				for (int e=1; e<eptCnt; e++)
				{
					CCyUSBEndPoint *ept = USBDevice->EndPoints[e];
					// INTR, BULK and ISO endpoints are supported.
					if ((ept->Attributes >= 1) && (ept->Attributes <= 3))
					{
						String *s = "";
						s = String::Concat(s, ((ept->Attributes == 1) ? "ISOC " :
							   ((ept->Attributes == 2) ? "BULK " : "INTR ")));
						s = String::Concat(s, ept->bIn ? "IN,       " : "OUT,   ");
						s = String::Concat(s, ept->MaxPktSize.ToString(), " Bytes,");
						if(USBDevice->BcdUSB == USB30MAJORVER)
							s = String::Concat(s, ept->ssmaxburst.ToString(), " MaxBurst,");

						s = String::Concat(s, "   (", i.ToString(), " - ");
						s = String::Concat(s, "0x", ept->Address.ToString("X02"), ")");
						EndPointsBox->Items->Add(s);
					}
				}
			}
		}
		if (EndPointsBox->Items->Count > 0 )
			EndPointsBox->SelectedIndex = 0;
		else
			StartBtn->Enabled = false;
	}
}
	#endif


int main()
{
	printf( "Test streamer\n" );
	struct CyprIO eps;
	int r = CyprIOConnect( &eps, 0, "\\\\?\\usb#vid_04b4&pid_00f1#5&c94d647&0&20#{ae18aa60-7f6a-11d4-97dd-00010229b959}" );
	if( r )
	{
		fprintf( stderr, "Error: Could not connect to USB device\n" );
		return -1;
	}
	r = CyprIOSetup( &eps );
	if( r )
	{
		fprintf( stderr, "Error: Could not setup USB device\n" );
		return -1;
	}
	printf( "Connected successfully\n" );
	
	return 0;
}
