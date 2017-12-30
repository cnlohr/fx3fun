#include <string.h>
#include <libcyprio.h>
#include <stdio.h>

#define DEBUGINFO(x...) fprintf( stderr, x );

static void CyprIOEPFromConfig( struct CyprIOEndpoint * ep, struct CyprIO * ths, int iface, PUSB_ENDPOINT_DESCRIPTOR epd, PUSB_CONFIGURATION_DESCRIPTOR cfg, PUSB_INTERFACE_DESCRIPTOR pIntfcDescriptor, USB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR* SSEndPtDescriptor )
{
	ZeroMemory( ep, sizeof(*ep) );
	ep->parent = ths;
	ep->assoc_iface = iface;

	ep->PktsPerFrame = (epd->wMaxPacketSize & 0x1800) >> 11;
	ep->PktsPerFrame++;

	ep->DscLen     = epd->bLength;
	ep->DscType    = epd->bDescriptorType;
	ep->Address    = epd->bEndpointAddress;
	ep->Attributes = epd->bmAttributes;
	ep->MaxPktSize = (epd->wMaxPacketSize & 0x7ff) * ep->PktsPerFrame;
	ep->Interval   = epd->bInterval;
	ep->bIn        = ((ep->Address & 0x80) == 0x80);

    ep->XferMode  = XMODE_DIRECT;  // Normally, use Direct xfers

	if( SSEndPtDescriptor )
	{
		ep->is_superspeed = 1;
		ep->ssdscLen =SSEndPtDescriptor->bLength;
		ep->ssdscType =SSEndPtDescriptor->bDescriptorType;
		ep->ssmaxburst=SSEndPtDescriptor->bMaxBurst; /* Maximum number of packets endpoint can send in one burst*/
		ep->MaxPktSize *= (ep->ssmaxburst+1);
		ep->ssbmAttribute=SSEndPtDescriptor->bmAttributes; // store endpoint attribute like for bulk it will be number of streams
		if((ep->Attributes & 0x03) ==1) // MULT is valid for Isochronous transfer only
			ep->MaxPktSize*=((SSEndPtDescriptor->bmAttributes & 0x03)+1); // Adding the MULT fields.

		ep->ssbytesperinterval=SSEndPtDescriptor->bBytesPerInterval;

	}
	else
	{
        // Initialize the SS companion descriptor with zero
        ep->ssdscLen =0;
        ep->ssdscType =0;
        ep->ssmaxburst=0; /* Maximum number of packets endpoint can send in one burst*/
        ep->ssbmAttribute=0; // store endpoint attribute like for bulk it will be number of streams
		ep->ssbytesperinterval=0;
	}
	
	printf( "ESPD (SS:%d): %d (len %d FL %d  %d)\n", ep->is_superspeed, epd->bmAttributes , ep->DscLen, ep->MaxPktSize, SSEndPtDescriptor->bMaxBurst );
}




int CyprIODoCircularDataXfer( struct CyprIOEndpoint * ep, int buffersize, int nrbuffers,  int (*callback)( void *, struct CyprIOEndpoint *, uint8_t *, uint32_t ), void * id )
{
	int i;
	int TimeOut = 1000;
	OVERLAPPED ovLapStatus[nrbuffers];
	uint8_t * buffers[nrbuffers];
	DWORD buflens[nrbuffers];
	DWORD dwReturnBytes = 0;

	int pkts;
	pkts = buffersize / ep->MaxPktSize;       // Number of packets implied by buffersize & pktSize
    if (buffersize % ep->MaxPktSize) pkts++;
    int iXmitBufSize = sizeof (SINGLE_TRANSFER) + (pkts * sizeof(ISO_PACKET_INFO));
	
	uint8_t * xmitbuffers[nrbuffers];
	PSINGLE_TRANSFER pTransfers[nrbuffers];
	
	for( i = 0; i < nrbuffers; i++ )
	{
		memset(&ovLapStatus[i],0,sizeof(OVERLAPPED));
		ovLapStatus[i].hEvent = CreateEvent(NULL, 0, 0, NULL);
		buffers[i] = (uint8_t*)malloc( buffersize );
		uint8_t * pXmitBuf = xmitbuffers[i] = (uint8_t*)malloc( iXmitBufSize );		
		pTransfers[i] = (PSINGLE_TRANSFER)pXmitBuf;
		ZeroMemory (pXmitBuf, iXmitBufSize);
		PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
		pTransfer->ucEndpointAddress = ep->Address;
		pTransfer->IsoPacketOffset = sizeof (SINGLE_TRANSFER);
		pTransfer->IsoPacketLength = pkts * sizeof(ISO_PACKET_INFO);
		pTransfer->BufferOffset = 0;
		pTransfer->BufferLength = 0;
	}
	
	for( i = 0; i < nrbuffers; i++ )
	{
		//Actually deploy all our requests.
		DeviceIoControl (ep->parent->hDevice,
			IOCTL_ADAPT_SEND_NON_EP0_DIRECT,
			xmitbuffers[i],
			iXmitBufSize,
			buffers[i], buffersize,
			&dwReturnBytes,
			&ovLapStatus[i]);
	}
	
	i = 0;
	do
	{
		//int wResult = WaitForIO( ep, &ovLapStatus[i], 1000 );
		LPOVERLAPPED l = &ovLapStatus[i];
        DWORD waitResult = WaitForSingleObject(l->hEvent,TimeOut);
		if( waitResult != WAIT_OBJECT_0 )
		{
			//Bad things happened.  Abort!
			DEBUGINFO( "Error: WaitForSingleObject returned %08x; LastError: %d\n", waitResult, GetLastError() );
			break;
		}
		DWORD bytes = 0;
		BOOL rResult = GetOverlappedResult(ep->parent->hDevice, l, &bytes, FALSE);
		//Look at ovLapStatus[i]
		if( !rResult )
		{
			int le = GetLastError();
			printf( "%d / %p / %p  %d BID:%d\n", l->Offset, l->hEvent, ep->parent->hDevice, rResult, i );
			DEBUGINFO( "Error: GetOverlappedResult returned with error %d\n", le);
			break;
		}
		
		//Got the data.  call our callback.
		if( bytes )
		{
			if( callback( id, ep, buffers[i], bytes ) )
				break;
		}

		//Hook that packet back up to our chain.
		DeviceIoControl (ep->parent->hDevice,
			IOCTL_ADAPT_SEND_NON_EP0_DIRECT,
			xmitbuffers[i],
			iXmitBufSize,
			buffers[i], buffersize,
			&dwReturnBytes,
			l);

			
		i++;
		if( i == nrbuffers ) i = 0;
	} while( 1 );
	
	for( i = 0; i < nrbuffers; i++ )
	{
		CloseHandle( ovLapStatus[i].hEvent );
		free( buffers[i] );
		free( xmitbuffers[i] );
	}
	return -1;
}






















int CyprIOConnect( struct CyprIO * ths, int index, const char * matching )
{
	int Devices = 0;
	
	ZeroMemory( ths, sizeof( *ths) );

    //SP_DEVINFO_DATA devInfoData;
    SP_DEVICE_INTERFACE_DATA  devInterfaceData;

    //Open a handle to the plug and play dev node.
    //SetupDiGetClassDevs() returns a device information set that contains info on all
    // installed devices of a specified class which are present.
	static GUID DrvGuid = {0xae18aa60, 0x7f6a, 0x11d4, 0x97, 0xdd, 0x0, 0x1, 0x2, 0x29, 0xb9, 0x59};
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
			uint32_t requiredLength = 0;
			SetupDiGetDeviceInterfaceDetailA ( hwDeviceInfo, &devInterfaceData, NULL, 0, &requiredLength, NULL);

			uint32_t predictedLength = requiredLength;

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
			//printf( "%s %s %d %p\n", DevPath, matching, index, strstr( DevPath, matching ) );
			if( strstr( DevPath, matching ) == 0 || index-- )
			{
				printf( "Matching\n" );
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


int CyprIOControl(struct CyprIO * ths, uint32_t cmd, uint8_t * XferBuf, uint32_t len)
{
    if ( ths->hDevice == INVALID_HANDLE_VALUE ) return 0;
    BOOL bDioRetVal = DeviceIoControl (ths->hDevice, cmd, XferBuf, len, XferBuf, len, &ths->BytesXferedLastControl, NULL);
    if(!bDioRetVal) { ths->LastError = GetLastError(); ths->BytesXferedLastControl = -1; }
	return !bDioRetVal || (ths->BytesXferedLastControl != len);
}

int CyprIOControlTransfer( struct CyprIO * ths, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char * data, uint16_t wLength, unsigned int timeout )
{
	DWORD received = 0;
	int buflen = sizeof( SINGLE_TRANSFER );
	//if( !(bmRequestType & 0x80) )
	{
		//out type tranfer.
		buflen += wLength;
	}
	uint8_t * buffer = (uint8_t*)malloc(buflen);  //XXX TODO: Figure out why this somethings makes things go bad if you alloca this.
	ZeroMemory( buffer, buflen );
	
	PSINGLE_TRANSFER st = (PSINGLE_TRANSFER)buffer;
    st->SetupPacket.bmRequest = bmRequestType;
    st->SetupPacket.bRequest = bRequest;
	st->SetupPacket.wValue = wValue;
	st->SetupPacket.wIndex = wIndex;
	st->SetupPacket.wLength = wLength;
	st->SetupPacket.ulTimeOut = (timeout/1000)+1;
	st->BufferLength = wLength;
	st->BufferOffset = sizeof( SINGLE_TRANSFER );
	st->ucEndpointAddress = 0;
	st->NtStatus = st->UsbdStatus = st->IsoPacketOffset = st->IsoPacketLength = 0;

	if( bmRequestType & 0x80 )
	{
		//Direction Device to Host (in)
		//int i; for( i = 0; i < buflen; i++ ) printf( "%02x ", buffer[i] );  printf( "\n ((%d))\n", IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER );
		int ret = DeviceIoControl (ths->hDevice, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buffer, buflen, buffer, buflen, &received, NULL);
		//int trueLength = (USB_COMMON_DESCRIPTOR*)((PCHAR)st + st->BufferOffset)->bLength;

		if( !ret || received < sizeof( SINGLE_TRANSFER ) )
		{
			ths->BytesXferedLastControl = -1;
			free( buffer );
			return -GetLastError();
		}
		received-=sizeof( SINGLE_TRANSFER ); 
		ths->BytesXferedLastControl = received;
		memcpy( data, buffer + sizeof( SINGLE_TRANSFER ), received );
		free( buffer );
		return received;
	}
	else
	{
		//Direction Host to Device (out)
		memcpy( buffer + sizeof( SINGLE_TRANSFER ), data, wLength );
		int ret = DeviceIoControl (ths->hDevice, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buffer, buflen, 0, 0, 0, NULL);
		if( !ret )
		{
			ths->BytesXferedLastControl = -1;
			free( buffer );
			return -GetLastError();
		}
		ths->BytesXferedLastControl = 0;
		free( buffer );
		return wLength-sizeof( SINGLE_TRANSFER );
	}
}


int CyprIOGetString( struct CyprIO * ths, wchar_t *str, uint8_t sIndex)
{
	uint8_t buffer[USB_STRING_MAXLEN+2];
	int ret = CyprIOControlTransfer( ths, 0x80, USB_REQUEST_GET_DESCRIPTOR, (USB_STRING_DESCRIPTOR_TYPE<<8) | sIndex, ths->StrLangID, buffer, sizeof( buffer ), 1000 );
	if( ret <= 0 )
	{
		return ret;
	}
	
	uint8_t bytes = buffer[0];
	uint8_t signature = buffer[1];
	
	if( ret>2  && signature == 0x03 )
	{
		memcpy(str, buffer+2, bytes-2);
		return 0;
	}
	return -2;
}


static int CyprIOGetInteralBOSDescriptor( struct CyprIO * ths )
{
	uint8_t buffer[1024];
	int ret = CyprIOControlTransfer( ths, 0x80, USB_REQUEST_GET_DESCRIPTOR, USB_BOS_DESCRIPTOR_TYPE<<8, 0, buffer, sizeof( buffer ), 1000 );
	if( ret <= 0 )
	{
		return ret;
	}
	ths->pUsbBosDescriptor = (PUSB_BOS_DESCRIPTOR)malloc( ret );
	memcpy( ths->pUsbBosDescriptor, buffer, ret );
	return 0;
}



static int PrintWString( wchar_t * ct )
{
	int i;
	for( i = 0; ct[i]; i++ )
	{
		printf( "%c", ct[i] );
	}		
}

static int CyprIOGetCfgDescriptor( struct CyprIO * ths, int descIndex )
{
	
	uint8_t buffer[1024];
	int ret = CyprIOControlTransfer( ths, 0x80, USB_REQUEST_GET_DESCRIPTOR, (USB_CONFIGURATION_DESCRIPTOR_TYPE<<8) | descIndex, 0, buffer, sizeof( buffer ), 1000 );
	printf( "RET: %d\n", ret );
	if( ret <= 0 )
	{
		return ret;
	}
	ths->USBConfigDescriptors[descIndex] = (PUSB_CONFIGURATION_DESCRIPTOR)malloc(ret);
	memcpy(ths->USBConfigDescriptors[descIndex], buffer, ret);
	return 0;
	
}


int CyprIOGetDevDescriptorInformation( struct CyprIO * ths )
{
	PSINGLE_TRANSFER pSingleTransfer;
	HANDLE hd = ths->hDevice;
	char buf[256];
	int bRetVal;
    uint32_t length;
	USB_COMMON_DESCRIPTOR cmnDescriptor;

	if( !hd )
	{
		DEBUGINFO( "Error: CyprIOGetDevDescriptorInformation called on a non-open device\n" );
		return -1;
	}
	
    //USB_DEVICE_DESCRIPTOR devDescriptor;

	int ret = CyprIOControlTransfer( ths, 0x80, USB_REQUEST_GET_DESCRIPTOR, (USB_DEVICE_DESCRIPTOR_TYPE<<8), 0, (uint8_t*)&ths->USBDeviceDescriptor, sizeof(USB_DEVICE_DESCRIPTOR), 2000 );
    if (ret<0) { DEBUGINFO( "Couldn't get cmnDesctiptor\n" ); return ret; }
	//printf( "Got USB Descriptor for: %04x:%04x\n", ths->USBDeviceDescriptor.idVendor, ths->USBDeviceDescriptor.idProduct );
	
	ret = CyprIOControlTransfer( ths, 0x80, USB_REQUEST_GET_DESCRIPTOR, (USB_STRING_DESCRIPTOR_TYPE<<8), 0, (uint8_t*)&cmnDescriptor, sizeof(USB_COMMON_DESCRIPTOR), 2000 );
    if (ret<0) { DEBUGINFO( "Couldn't get cmnDesctiptor\n" ); return ret; }

	int LangIDs = (cmnDescriptor.bLength - 2 ) / 2;
	ret = CyprIOControlTransfer( ths, 0x80, USB_REQUEST_GET_DESCRIPTOR, (USB_STRING_DESCRIPTOR_TYPE<<8), 0, (uint8_t*)buf, sizeof( buf ), 2000 );
    if (ret<0) { DEBUGINFO( "Couldn't get cmnDesctiptor\n" ); return ret; }

	PUSB_STRING_DESCRIPTOR IDs = (PUSB_STRING_DESCRIPTOR)buf;

	ths->StrLangID =(LangIDs) ? IDs[0].bString[0] : 0;

	for (int i=0; i<LangIDs; i++) {
		uint16_t id = IDs[i].bString[0];
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
	if( CyprIOControl( ths, IOCTL_ADAPT_GET_ADDRESS, &ths->USBAddress, 1L) )
	{
		DEBUGINFO ("Failed to get USB address\n" );
		return 1;
	}

    ZeroMemory(ths->FriendlyName, USB_STRING_MAXLEN);
	CyprIOControl(ths, IOCTL_ADAPT_GET_FRIENDLY_NAME, (PUCHAR)ths->FriendlyName, USB_STRING_MAXLEN);
	if( ths->BytesXferedLastControl <= 0 )
	{
		DEBUGINFO( "Failed to get friendly name %d\n", ths->LastError );
		return 1;
	}
	//You can also get IOCTL_ADAPT_GET_DEVICE_NAME the same way but I don't care for it.
	//Also bool bRetVal = IoControl(IOCTL_ADAPT_GET_DRIVER_VERSION, (PUCHAR) &DriverVersion, sizeof(ULONG)); for Driver version.
	//Also bool bRetVal = IoControl(IOCTL_ADAPT_GET_USBDI_VERSION, (PUCHAR) &USBDIVersion, sizeof(ULONG));

	
	uint32_t speed = 0;
	int bHighSpeed = 0;
	int bSuperSpeed = 0;
	CyprIOControl( ths, IOCTL_ADAPT_GET_DEVICE_SPEED, (PUCHAR)&speed, sizeof(speed));
	bHighSpeed = (speed == DEVICE_SPEED_HIGH);
	bSuperSpeed = (speed == DEVICE_SPEED_SUPER);
	if( !bSuperSpeed )
	{
		DEBUGINFO( "Error: Super speed bit not set!\n" );
		return -3;
	}

	int configs = ths->USBDeviceDescriptor.bNumConfigurations;
	int i;
	printf( "Found %d configs\n", configs );
	for ( i=0; i < configs; i++)
	{
		if( CyprIOGetCfgDescriptor( ths, i ) )
		{
			DEBUGINFO( "Could not get config %d\n", i );
			return -1;
		}
		printf( "Got Config (length %d): (Value: %d)\n", ths->USBConfigDescriptors[i]->wTotalLength, ths->USBConfigDescriptors[i]->bConfigurationValue );
		int k;
		uint8_t * p = (uint8_t*)ths->USBConfigDescriptors[i];
		for( k = 0; k < ths->USBConfigDescriptors[i]->wTotalLength; k++)
		{
			printf( " %02x", p[k] );
		}
		printf ("\n" );
	}
	printf( "Selected %d\n", i );
	
	if( !ths->USBConfigDescriptors[0] )
	{
	   DEBUGINFO( "Can't get a good interface\n" );
	   return -1;
	}

	printf( "Successfully connected to: %s\n", ths->FriendlyName );
	return 0;
}


int CyprIOSetup( struct CyprIO * ths, int use_config, int use_iface )
{
	HANDLE hd = ths->hDevice;
	if( !hd )
	{
		DEBUGINFO( "Error: CyprIOSetup called on a non-open device\n" );
		return -1;
	}
	/* The folllowing code is kind of after of the 
			for (int i=0; i<Configs; i++) 
				{
					GetCfgDescriptor(i);
		section in CyAPI.cpp. */
		
	//What actually happens there is it instantiates a number of classes
	//based on the contents in USBConfigDescriptors.  This feels like considerable
	//overkill.  So, I'm shooting for making this really simple.

	//All the device device info is now enumerated.  Need to actually do stuff.
	//Honestly, I don't understand the value of this section of code I've translated.
	int foundconfig = -1;
	int nrdesc = sizeof( ths->USBConfigDescriptors ) / sizeof( ths->USBConfigDescriptors[0] );
	int AltInterfaces = 0;
	int ifaces = 0;
	
	int use_this_config = 0;
	int use_this_interface = 0;

	//This code mimics what's going on in CCyUSBConfig::CCyUSBConfig(...).
	if( !ths->USBConfigDescriptors[use_config] )
	{
		DEBUGINFO( "Config %d does not exist\n", use_config );
		return -1;
	}
	
	
	PUSB_CONFIGURATION_DESCRIPTOR pConfigDescr = ths->USBConfigDescriptors[use_config];
	int tLen = pConfigDescr->wTotalLength;
	PUCHAR desc = (PUCHAR)pConfigDescr;
	int bytesConsumed = pConfigDescr->bLength;

	printf( "Looping:\n");
	
	int totallen = pConfigDescr->wTotalLength;

	int totalread = 0;
	
	PUCHAR end = desc + tLen;
	desc += pConfigDescr->bLength;
	
	while( desc < end && ifaces < MAX_INTERFACES)
	{
		PUSB_INTERFACE_DESCRIPTOR interfaceDesc = (PUSB_INTERFACE_DESCRIPTOR) (desc);
		desc += interfaceDesc->bLength;
		ths->USBIfaceDescriptors[ifaces] = interfaceDesc;
		
		if (interfaceDesc->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE)
		{
			if( ifaces == use_iface )
				printf ("*" );
			else
				printf (" ");
			printf( "Iface: %d:%d = %d / %d eps\n", use_config, ifaces, interfaceDesc->bDescriptorType, interfaceDesc->bNumEndpoints );

			int endpoints = interfaceDesc->bNumEndpoints;
			int wTotalLength = interfaceDesc->bLength;
			
			//Mostly from CCyUSBInterface::CCyUSBInterface(HANDLE handle, PUSB_INTERFACE_DESCRIPTOR pIntfcDescriptor)
			//PUCHAR epdesc = (PUCHAR)interfaceDesc  + interfaceDesc->bLength;
			int ep;
			
			for( ep = 0; ep < endpoints && desc < end; ep++ )
			{	
				PUSB_ENDPOINT_DESCRIPTOR  endPtDesc = (PUSB_ENDPOINT_DESCRIPTOR) (desc);
				desc += endPtDesc->bLength;

				if (endPtDesc->bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE) {
					PUSB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR ssdesc = (PUSB_SUPERSPEED_ENDPOINT_COMPANION_DESCRIPTOR) (desc);
					desc += ssdesc->bLength;

					if( ifaces == use_iface )
					{
						printf( " *" );
						CyprIOEPFromConfig( &ths->CypIOEndpoints[ths->CyprIONumEps++], ths, ifaces, endPtDesc, pConfigDescr, interfaceDesc, ssdesc );
					}
					else
					{
						printf ("  ");
						//Even if we aren't using this interface, still want to print out info about it.
						struct CyprIOEndpoint dummyep;
						CyprIOEPFromConfig( &dummyep, ths, ifaces, endPtDesc, pConfigDescr, interfaceDesc, ssdesc );
					}
				}
				else
					break;				
			}

			ifaces++;
			AltInterfaces++;  // Actually the total number of interfaces for the config
		} 
	}


    UCHAR alt;
	if (CyprIOControl(ths,IOCTL_ADAPT_GET_ALT_INTERFACE_SETTING, &alt, 1))
	{
		DEBUGINFO( "Error: can't get alt interface setting\n" );
	}
	//Now, we write code that mimics SetConfig(...)??
	int configs = ths->USBDeviceDescriptor.bNumConfigurations;
	CyprIOControl(ths, IOCTL_ADAPT_SELECT_INTERFACE, &use_iface, 1L);
	
	//Isn't there some sort of "claim" operation that needs to happen?

	return 0;
}
