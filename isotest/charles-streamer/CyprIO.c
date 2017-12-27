#include <string.h>
#include <CyprIO.h>
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




static BOOL Abort( struct CyprIOEndpoint * ep )
{   
	DWORD dwBytes = 0;
	BOOL  RetVal = FALSE;
	OVERLAPPED ov;

	memset(&ov,0,sizeof(ov));
	ov.hEvent = CreateEvent(NULL,0,0,NULL);
	
	RetVal  = (DeviceIoControl(ep->parent->hDevice,
		IOCTL_ADAPT_ABORT_PIPE,
		&ep->Address,
		sizeof(UCHAR),
		NULL,
		0,
		&dwBytes,
		&ov)!=0);
	if(!RetVal)
	{
		DWORD LastError = GetLastError();
		if(LastError == ERROR_IO_PENDING)
			WaitForSingleObject(ov.hEvent,INFINITE);
	}
	CloseHandle(ov.hEvent);
	return 1;

}
//________


static PUCHAR BeginDirectXfer(struct CyprIOEndpoint * ep, PUCHAR buf, LONG bufLen, OVERLAPPED *ov, uint8_t * xmitinplace )
{
    if ( ep->parent->hDevice == INVALID_HANDLE_VALUE ) return NULL;

	int pkts;
	if(ep->MaxPktSize)
		pkts = bufLen / ep->MaxPktSize;       // Number of packets implied by bufLen & pktSize
	else
	{
		pkts = 0;
		return NULL;
	}

    if (bufLen % ep->MaxPktSize) pkts++;
    if (pkts == 0) return NULL;

    int iXmitBufSize = sizeof (SINGLE_TRANSFER) + (pkts * sizeof(ISO_PACKET_INFO));
    UCHAR * pXmitBuf;
	if( xmitinplace )
		pXmitBuf = xmitinplace;
	else
		pXmitBuf = (UCHAR*) malloc(iXmitBufSize);
    ZeroMemory (pXmitBuf, iXmitBufSize);

    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
    pTransfer->ucEndpointAddress = ep->Address;
    pTransfer->IsoPacketOffset = sizeof (SINGLE_TRANSFER);
    pTransfer->IsoPacketLength = pkts * sizeof(ISO_PACKET_INFO);
    pTransfer->BufferOffset = 0;
    pTransfer->BufferLength = 0;

    DWORD dwReturnBytes = 0;


    int ret = DeviceIoControl (ep->parent->hDevice,
        IOCTL_ADAPT_SEND_NON_EP0_DIRECT,
        pXmitBuf,
        iXmitBufSize,
        buf,
        bufLen,
        &dwReturnBytes,
        ov);

    // Note that this method leaves pXmitBuf allocated.  It will get deleted in
    // FinishDataXfer.

    int LastError = GetLastError();
	
    return pXmitBuf;
}

static PUCHAR BeginDataXfer(struct CyprIOEndpoint * ep, uint8_t * buf, LONG bufLen, OVERLAPPED *ov)
{
    if ( ep->parent->hDevice == INVALID_HANDLE_VALUE ) return NULL;

    if (ep->XferMode == XMODE_DIRECT)
        return BeginDirectXfer( ep, buf, bufLen, ov, 0);
    else
	{
		DEBUGINFO( "Error: Buffered mode not yet supported!\n" );
		return 0;
		//return BeginBufferedXfer(buf, bufLen, ov);
	}
}


static BOOL WaitForIO( struct CyprIOEndpoint * ep, OVERLAPPED *ovLapStatus, int TimeOut)
{
    int LastError = GetLastError();

    if (LastError == ERROR_SUCCESS) return 1;  // The command completed

    if (LastError == ERROR_IO_PENDING) {
        DWORD waitResult = WaitForSingleObject(ovLapStatus->hEvent,TimeOut);

        if (waitResult == WAIT_OBJECT_0) return 1;  

        if (waitResult == WAIT_TIMEOUT) 
		{			
			Abort( ep );
			//// Wait for the stalled command to complete - should be done already
			DWORD  retcode = WaitForSingleObject(ovLapStatus->hEvent,50); // Wait for 50 milisecond
	
			if(retcode == WAIT_TIMEOUT || retcode==WAIT_FAILED) 
			{// Worst case condition , in multithreaded environment if user set time out to ZERO and cancel the IO the requiest, rarely first Abort() fail to cancel the IO, so reissueing second Abort(0.				
				Abort( ep ); 
				retcode = WaitForSingleObject(ovLapStatus->hEvent,INFINITE); 
				
			}
        }
    }

    return 0;
}


static int FinishDataXfer(struct CyprIOEndpoint * ep, PUCHAR buf, LONG *bufLen, OVERLAPPED *ov, PUCHAR pXmitBuf, struct CyprCyIsoPktInfo* pktInfos)
{
    DWORD bytes = 0;
    BOOL rResult = (GetOverlappedResult(ep->parent->hDevice, ov, &bytes, FALSE)!=0);

    PSINGLE_TRANSFER pTransfer = (PSINGLE_TRANSFER) pXmitBuf;
    *bufLen = (bytes) ? bytes - pTransfer->BufferOffset : 0;
    //bytesWritten = bufLen;

    //UsbdStatus = pTransfer->UsbdStatus;
    //NtStatus   = pTransfer->NtStatus;

    if (ep->bIn && (ep->XferMode == XMODE_BUFFERED) && (bufLen > 0)) {
        UCHAR *ptr = (PUCHAR)pTransfer + pTransfer->BufferOffset;
        memcpy (buf, ptr, *bufLen);
    }

    // If a buffer was provided, pass-back the Isoc packet info records
    if (pktInfos && (bufLen > 0)) {
        ZeroMemory(pktInfos, pTransfer->IsoPacketLength);
        PUCHAR pktPtr = pXmitBuf + pTransfer->IsoPacketOffset;
        memcpy(pktInfos, pktPtr, pTransfer->IsoPacketLength);
    }

    free( pXmitBuf );     // [] Changed in 1.5.1.3

    return rResult;
}

#if 0
int CyprDataXfer( struct CyprIOEndpoint * ep, uint8_t * buf, uint32_t *bufLen, struct CyprCyIsoPktInfo* pktInfos)
{
    OVERLAPPED ovLapStatus;
    memset(&ovLapStatus,0,sizeof(OVERLAPPED));

    ovLapStatus.hEvent = CreateEvent(NULL, 0, 0, NULL);

    PUCHAR context = BeginDataXfer( ep, buf, *bufLen, &ovLapStatus);
    int   wResult = WaitForIO(ep, &ovLapStatus, 1000 );
    int   fResult = FinishDataXfer(ep, buf, bufLen, &ovLapStatus, context, pktInfos);

    CloseHandle(ovLapStatus.hEvent);

    return wResult && fResult;
}
#endif

int CyprDataXfer( struct CyprIOEndpoint * ep, uint8_t * buf, uint32_t *bufLen, struct CyprCyIsoPktInfo* pktInfos)
{
    OVERLAPPED ovLapStatus;
    memset(&ovLapStatus,0,sizeof(OVERLAPPED));

    ovLapStatus.hEvent = CreateEvent(NULL, 0, 0, NULL);

    PUCHAR context = BeginDataXfer( ep, buf, *bufLen, &ovLapStatus);
    int   wResult = WaitForIO(ep, &ovLapStatus, 1000 );
    int   fResult = FinishDataXfer(ep, buf, bufLen, &ovLapStatus, context, pktInfos);

    CloseHandle(ovLapStatus.hEvent);

    return wResult && fResult;
}


int CyprIODoCircularDataXfer( struct CyprIOEndpoint * ep, int buffersize, int nrbuffers,  int (*callback)( void *, struct CyprIOEndpoint *, uint8_t *, uint32_t ), void * id )
{
	int i;
	int TimeOut = 1000;
	OVERLAPPED ovLapStatus[nrbuffers];
	uint8_t * buffers[nrbuffers];
	DWORD buflens[nrbuffers];
	

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
		xmitbuffers[i] = (uint8_t*)malloc( iXmitBufSize );
		
		pTransfers[i] = (PSINGLE_TRANSFER)BeginDirectXfer( ep,
			buffers[i],
			buffersize,
			&ovLapStatus[i], xmitbuffers[i] );
	}
	
	int bid = 0;
	do
	{
		//int wResult = WaitForIO( ep, &ovLapStatus[bid], 1000 );
		LPOVERLAPPED l = &ovLapStatus[bid];
        DWORD waitResult = WaitForSingleObject(l->hEvent,TimeOut);
		if( waitResult != WAIT_OBJECT_0 )
		{
			//Bad things happened.  Abort!
			DEBUGINFO( "Error: WaitForSingleObject returned %08x; LastError: %d\n", waitResult, GetLastError() );
			break;
		}
		DWORD bytes = 0;
		BOOL rResult = GetOverlappedResult(ep->parent->hDevice, l, &buflens[bid], FALSE);
		//Look at ovLapStatus[bid]
		if( !rResult )
		{
			int le = GetLastError();
			printf( "%d / %p / %p  %d BID:%d\n", l->Offset, l->hEvent, ep->parent->hDevice, rResult, bid );
			DEBUGINFO( "Error: GetOverlappedResult returned with error %d\n", le);
			break;
		}
		
		//Note: Theoretically, you can read the data out with         UCHAR *ptr = (PUCHAR)pTransfer + pTransfer->BufferOffset; I think.
		//PSINGLE_TRANSFER pTransfer = pTransfers[bid];
		//UCHAR *ptr = (PUCHAR)pTransfer + pTransfer->BufferOffset;
		
		if( callback( id, ep, buffers, buflens[bid] ) )
			break;
		
		BeginDirectXfer( ep,
			buffers[bid],
			buffersize,
			&ovLapStatus[bid], xmitbuffers[bid] );
			/*
		int ret = DeviceIoControl (ep->parent->hDevice,
			IOCTL_ADAPT_SEND_NON_EP0_DIRECT,
			pXmitBuf,
			iXmitBufSize,
			buf,
			bufLen,
			&dwReturnBytes,
			l);*/

		bid++;
		if( bid == nrbuffers ) bid = 0;
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
	
	
	

int CyprIoControl(struct CyprIO * ths, ULONG cmd, uint8_t * XferBuf, uint32_t len)
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

static int CyprIOGetCfgDescriptor( struct CyprIO * ths, int descIndex )
{
	if( descIndex > sizeof( ths->USBConfigDescriptors ) / sizeof( ths->USBConfigDescriptors[0] ) )
		return -2;
	
    ULONG length = sizeof(SINGLE_TRANSFER) + sizeof(USB_CONFIGURATION_DESCRIPTOR);
    UCHAR buf[length];
	int bRetVal;
    PSINGLE_TRANSFER pSingleTransfer;

    ZeroMemory(buf, length);

	pSingleTransfer = FillSingleControlTransfer( buf, USB_REQUEST_GET_DESCRIPTOR, USB_CONFIGURATION_DESCRIPTOR_TYPE, descIndex, 0, sizeof(USB_CONFIGURATION_DESCRIPTOR) );
	bRetVal = CyprIoControl(ths, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, length);
    if (bRetVal) {
		return -1;
	}
	PUSB_CONFIGURATION_DESCRIPTOR configDesc = (PUSB_CONFIGURATION_DESCRIPTOR)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset);
	USHORT configDescLength= configDesc->wTotalLength;

	length = sizeof(SINGLE_TRANSFER) + configDescLength;
	UCHAR buf2[length];
	ZeroMemory (buf2, length);
	pSingleTransfer = FillSingleControlTransfer( buf2, USB_REQUEST_GET_DESCRIPTOR, USB_CONFIGURATION_DESCRIPTOR_TYPE, descIndex, 0, configDescLength );
	bRetVal = CyprIoControl(ths, IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf2, length);
    if (bRetVal) {
		return -1;
	}
	ths->USBConfigDescriptors[descIndex] = (PUSB_CONFIGURATION_DESCRIPTOR)malloc(configDescLength);
	memcpy(ths->USBConfigDescriptors[descIndex], (PVOID)((PCHAR)pSingleTransfer + pSingleTransfer->BufferOffset), configDescLength);
	return 0;
}


int CyprIOGetDevDescriptorInformation( struct CyprIO * ths )
{
	PSINGLE_TRANSFER pSingleTransfer;
	HANDLE hd = ths->hDevice;
	char buf[80];
	int bRetVal;
    ULONG length;
	USB_COMMON_DESCRIPTOR cmnDescriptor;

	if( !hd )
	{
		DEBUGINFO( "Error: CyprIOGetDevDescriptorInformation called on a non-open device\n" );
		return -1;
	}
	
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
	if (CyprIoControl(ths,IOCTL_ADAPT_GET_ALT_INTERFACE_SETTING, &alt, 1))
	{
		DEBUGINFO( "Error: can't get alt interface setting\n" );
	}
	//Now, we write code that mimics SetConfig(...)??
	int configs = ths->USBDeviceDescriptor.bNumConfigurations;

	//Isn't there some sort of "claim" operation that needs to happen?

	return 0;
}
