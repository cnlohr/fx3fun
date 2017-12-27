#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "CyprIO.h"

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
	r = CyprIOSetup( &eps, 0, 1 );
	if( r )
	{
		fprintf( stderr, "Error: Could not setup USB device\n" );
		return -1;
	}
	

	{
		uint8_t buf[16384];
		uint32_t bufLen = sizeof(buf);
		struct CyprCyIsoPktInfo Status;
		CyprDataXfer( &eps.CypIOEndpoints[0], buf, &bufLen, &Status);
		printf( "%02x\n", buf[0] );
	}
	
	
	printf( "Connected successfully\n" );
	
	return 0;
}
