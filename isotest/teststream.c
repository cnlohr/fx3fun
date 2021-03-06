/*
	Tool to test Cypress FX3 isochronous transfers using the stream test firmware
	(C) 2017 C. Lohr, under the MIT-x11 or NewBSD License.  You decide.
	Tested on Windows, working full functionality 12/30/2017
	Tested on Linux, working full functionality 7/22/2018
*/

#include <stdio.h>
#include <stdlib.h>
#include <libcyprio.h>
#include "os_generic.h"

#if defined(WINDOWS) || defined( WIN32 )
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

double Last;
double bytes;

struct CyprIO eps;


void TickCypr()
{
	char buf[64];
	printf( "Control\n" );
	int e = CyprIOControlTransfer( &eps, 0xc0, 0xaa, 0x0102, 0x0304, buf, 50, 10 );
	printf( "GOT: %d:", e );
	int i;
	for( i = 0; i < e; i++ )
	{
		printf( "%02x ", (uint8_t)buf[i] );
	}
	printf( "\n" );
}


void * TickThread( void * v )
{
	while(1)
	{
		TickCypr();
		OGSleep(1);
	}
}

int callback( void * id, struct CyprIOEndpoint * ep, uint8_t * data, uint32_t length )
{
	bytes += length;
	double Now = OGGetAbsoluteTime();
	//if( data[0] != 0xaa ) printf( "Bad data\n" );
	//printf( "%d %02x %02x\n", length, data[0], data[100] );
	if( Last + 1 < Now )
	{
		printf( "Got %.3f kB/s %d\n", bytes/1000.0, length );
//		int j;
//		for( j = 0; j < length; j += 8192 )
//		{
//			printf( "[%02x %02x]", data[j+0], data[j+1] );
//		}
		Last++;
		bytes = 0;

	}

	int j;
	for( j = 0; j < length; j += 8192 )
	{
		if( data[j] == 0 )
		{
			printf( "Zero at: %d\n",  j );
			return 0;
		}
	}
	return 0;
}

#if !defined(WINDOWS) && !defined(WIN32)
void CtrlCSignal()
{
	eps.shutdown = 1;
	usleep(100000);
	CyprIODestroy( &eps );
}
#endif


int main()
{
	printf( "Test streamer\n" );
#if !defined(WINDOWS) && !defined(WIN32)
	signal(SIGINT, CtrlCSignal);
#endif
	int r = CyprIOConnect( &eps, 0, 0x04b4, 0x00f1 );
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
			printf( "%.3f kB/s\n", bytes/1000.0 );
			bytes = 0;
		}
		#if 0
		for( i = 0; i < bufLen; i++ )
		{
			printf( "%02x ", buf[i] );
		}
		#endif
	}
	
#elif 0

	int i;
	for( i = 0; i < 10; i++ )
	{
		char buf[64];
		memcpy( buf, "hello", 5 );
		//CyprIOControl( ep->parent, 0xc0, buf, 5 );
//		PSINGLE_TRANSFER pSingleTransfer = FillSingleControlTransfer( buf, 0xc0, 0xc0, 0xc0, 0xcc, 5 );
//		int e = CyprIOControl( &eps.CypIOEndpoints[0], IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER, buf, sizeof(SINGLE_TRANSFER)+2 );
//		int e = CyprIOControl( &eps.CypIOEndpoints[0], 0x40, buf, 5 );
		int e = CyprIOControlTransfer( &eps, 0xc0, 0xaa, 0x0102, 0x0304, buf, 50, 5000 );
		printf( "GOT: %d:", e );
		int i;
		for( i = 0; i < e; i++ )
		{
			printf( "%02x ", (uint8_t)buf[i] );
		}
		printf( "\n" );
	}
	return 0;

#else
		
	Last = OGGetAbsoluteTime();
	
	OGCreateThread( TickThread, 0 );
#if defined( WINDOWS ) || defined( WIN32)
	CyprIODoCircularDataXferTx( &eps.CypIOEndpoints[0], 65536*16, 8,  callback, 0 );
#else
	CyprIODoCircularDataXferTx( &eps.CypIOEndpoints[0], 32768, 16,  callback, 0 );
#endif
	printf( "Done with circular data xfer\n" );

#endif	
	
	
	return 0;
}
