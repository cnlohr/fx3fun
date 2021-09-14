/*
 Connect a wire from CTRL[8] to D[0]
 
 This will toggle it and time the amount of time it takes to change state
 and see change.
 
 */

#include <stdio.h>
#include "libcyprio.h"
#include "os_generic.h"


struct CyprIO eps;

int bytes;

double RiseTime = 0;
int last1;
int callback( void * id, struct CyprIOEndpoint * ep, uint8_t * data, uint32_t length )
{
	bytes += length;
	double Now = OGGetAbsoluteTime();
	uint8_t final = data[length-1];
	if( (!!(final&1)) != (!!(last1&1)) )
	{
		if( !!(final&1) )
			RiseTime = OGGetAbsoluteTime();
	}
	last1 = final;
	return 0;
}
void * CallbackThread( void * v )
{
	while( 1 )
	{
		printf( "Callback Thread\n" );
#if defined( WINDOWS ) || defined( WIN32)
		CyprIODoCircularDataXferTx( &eps.CypIOEndpoints[0], 65536*4, 32,  callback, 0 );
#else
		CyprIODoCircularDataXferTx( &eps.CypIOEndpoints[0], 16834, 16,  callback, 0 );
#endif
		printf( "CALLBACK THREAD FAILED\n" );
		OGUSleep( 100000 );
	}
}

#define DDCMD( opcode, par1, par2, par3 )  ( e = CyprIOControlTransfer( &eps, 0xc0, 0xdd, (opcode) | ((par1)<<8), (par2) | ((par3)<<8), buffer, 48, 10 ), buffer[0] )

int main()
{
	uint8_t buffer[64];
	int e;

	int r = CyprIOConnect( &eps, 0, 0x04b4, 0x00f1 );
	if( r )
	{
		fprintf( stderr, "Error: Could not connect to USB device\n" );
		return;
	}
	printf( "CYPRIO CONNECT: %d\n", r );
	r = CyprIOGetDevDescriptorInformation( &eps );
	if( r )
	{
		fprintf( stderr, "Error: Couldn't get all info needed from device\n" );
		CyprIODestroy( &eps );
		return;
	}
	printf( "CONNECT 3: %d\n", r );
	r = CyprIOSetup( &eps, 0, 2 );
	printf( "Cyprio setup returned: %d\n", r );

	
	OGUSleep( 20000 );

	printf( "Doing data XFERs\n" );
	DDCMD( 0x10, 32/*divisor*/, 2 /*bitfield &1 = half div, &2 = use DLL clock*/, 1 /*GPIF Selected*/ ); 
	printf( "DDCMD FOR 0x10 (speed drive) %d  [%02x %02x %02x]\n", e, buffer[0], buffer[1], buffer[2] );

	OGCreateThread( CallbackThread, 0 );
	OGUSleep( 20000 );

	e = CyprIOControlTransfer( &eps, 0xc0, 0xbb, 0x0102, 0x0304, buffer, 50, 10 );
	OGUSleep( 20000 );
	e = CyprIOControlTransfer( &eps, 0xc0, 0xaa, 0x0102, 0x0304, buffer, 50, 10 );
	
	double Last;
	double SumTimes = 0;
	int counts = 0;
	while( 1 )
	{
		DDCMD( 0x01, 25, 2, 0 ); //Drive low
		OGUSleep( 40000 );
		double TimeCommandLow = OGGetAbsoluteTime();
		DDCMD( 0x01, 25, 9, 0 ); //Drive high
		OGUSleep( 40000 );
		double Now = OGGetAbsoluteTime();
		SumTimes += RiseTime - TimeCommandLow;
		counts++;
		printf( "%f : AVG %f ms %d %f\n", (RiseTime - TimeCommandLow)*1000, SumTimes/counts*1000, bytes, bytes/(Now-Last)  ); bytes = 0;
		Last=Now;
	}
	CyprIODestroy( &eps );
}
