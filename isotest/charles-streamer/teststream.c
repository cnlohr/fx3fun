#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "CyprIO.h"
#include "os_generic.h"

double Last;
double bytes;

int callback( void * id, struct CyprIOEndpoint * ep, uint8_t * data, uint32_t length )
{
	bytes += length;
	double Now = OGGetAbsoluteTime();
	//if( data[0] != 0xaa ) printf( "Bad data\n" );
	//printf( "%d %02x %02x\n", length, data[0], data[100] );
	if( Last + 1 < Now )
	{

		printf( "Got %.3f kB/s [%02x %02x]\n", bytes/1000, data[0], data[1] );
		Last++;
		bytes = 0;
	}
	return 0;
}


int main()
{
	printf( "Test streamer\n" );
	struct CyprIO eps;
	int r = CyprIOConnect( &eps, 0, "\\\\?\\usb#vid_04b4&pid_00f1#" ); //5&c94d647&0&20#{ae18aa60-7f6a-11d4-97dd-00010229b959}" );
	if( r )
	{
		fprintf( stderr, "Error: Could not connect to USB device\n" );
		return -1;
	}
	r = CyprIOGetDevDescriptorInformation( &eps );
	if( r )
	{
		fprintf( stderr, "Error: Couldn't get all info needed from device\n" );
		return -1;
	}
	r = CyprIOSetup( &eps, 0, 2 );
	if( r )
	{
		fprintf( stderr, "Error: Could not setup USB device\n" );
		return -1;
	}
	
	printf( "Connected successfully\n" );
#if 0
	Last = OGGetAbsoluteTime();
	bytes = 0;
	while(1)
	{
		uint8_t buf[32768];
		uint32_t bufLen = sizeof(buf);
		struct CyprCyIsoPktInfo Status;
		CyprDataXfer( &eps.CypIOEndpoints[0], buf, &bufLen, &Status);
		int i;
		//printf( "%d\n", bufLen );
		bytes += bufLen;
		
		double Now = OGGetAbsoluteTime();
		if( Last + 1 < Now )
		{
			Last++;
			printf( "%.3f KB/s\n", bytes/1024.0 );
			bytes = 0;
		}
		#if 0
		for( i = 0; i < bufLen; i++ )
		{
			printf( "%02x ", buf[i] );
		}
		#endif
	}
	
#else
	
#if 0
	int i;
	for( i = 0; i < 10; i++ )
	{
		char buf[64];
		memcpy( buf, "hello", 5 );
		//CyprIOControl( ep->parent, 0xc0, buf, 5 );
		PSINGLE_TRANSFER pSingleTransfer = FillSingleControlTransfer( buf, 0xc0, 0xc0, 0xc0, 0xcc, 5 );
		int e = CyprIOControl( &eps.CypIOEndpoints[0], IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, sizeof(SINGLE_TRANSFER)+2 );
//		int e = CyprIOControl( &eps.CypIOEndpoints[0], 0x40, buf, 5 );
		printf( "GOT: %d\n", e );
	}
	#endif
	Last = OGGetAbsoluteTime();
	CyprIODoCircularDataXfer( &eps.CypIOEndpoints[0], 65536*16, 8,  callback, 0 );


#endif	
	
	
	return 0;
}
