/*
	Tool to test Cypress FX3 isochronous transfers using the stream test firmware
	(C) 2017-2020 C. Lohr, under the MIT-x11 or NewBSD License.  You decide.
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
		OGSleep(1);
		TickCypr();
	}
}

uint8_t * datalog;
int capsize;
int second_in;
int callback( void * id, struct CyprIOEndpoint * ep, uint8_t * data, uint32_t length )
{
	bytes += length;
	double Now = OGGetAbsoluteTime();
	//if( data[0] != 0xaa ) printf( "Bad data\n" );
	//printf( "%d %02x %02x\n", length, data[0], data[100] );
	if( Last + 1 < Now )
	{
		printf( "Got %.3f kB/s [%02x %02x]\n", bytes/1000.0, data[0], data[1] );
		Last++;
		bytes = 0;
		second_in++;
	}
	//if( second_in < 2 ) return 0;

	static int tlen;
	int tocopy = length;
	if( length + tlen > capsize )
	{
		tocopy = capsize - tlen;
	}
	memcpy( ((uint8_t*)datalog)+tlen, data, tocopy );
	tlen += tocopy;

	if( tlen >= capsize )
	{
		printf( "CAPTURED\n" );
		int confidence;
		int last = 0;
		int lr = 0;
		int lastcount = 0;
		FILE * f = fopen( "testdata.txt", "w" );
		int j;
		int good = 0, total = tlen/2;
		for( j = 0; j < tlen/2; j++ )
		{
			//Spurious 0 set?  Skip.
			int imark = j/16384;
			int selected;
			//selected = (!(imark&1))?(j-32768):(j+32768);
			selected = j;
			if( selected < 0 ) selected = 0;
			uint8_t d = datalog[selected];
			
			if( (selected&0x3fff) == 0 && !d) { printf( "BAD %d %d\n", j	, j / 16384 ); }
			good++;
#if 1
			if( (j & 0x3fff) == 30 ) printf( "%d, %d\n", j/16384, (datalog[selected+1]^d)>>6 );
			fprintf( f, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", 
				j/16384, (datalog[selected+1]^d)>>6,
				(d & 1)?1:0,
				(d & 2)?1:0,
				(d & 4)?1:0,
				(d & 8)?1:0,
				(d & 16)?1:0,
				(d & 32)?1:0,
				(d & 64)?1:0,
				(d & 128)?1:0 );

			if( (d&4) != last )
			{
				confidence--;
				if( confidence < 1 )
				{
					last = (d&4);
					if( lastcount < 59 || ( lastcount > 63 && lastcount < 180 ) || lastcount > 185 )
						fprintf( f, "SPLIT: %d  ", lastcount );
					lastcount = 1;
				}
			}
			else
			{
				confidence = 2;
				lastcount++;
			}

#elif 0
			fprintf( f, "%d\n", 
				(d & 8)?1:0 );

#else
			//if( ( j & 16383 ) > 10 && ( j & 16383 ) < 15)
			{
				fprintf( f, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", 
				j/16384,
				(d & 1)?1:0,
				(d & 2)?1:0,
				(d & 4)?1:0,
				(d & 8)?1:0,
				(d & 16)?1:0,
				(d & 32)?1:0,
				(d & 64)?1:0,
				(d & 128)?1:0 );

			}
#endif
		}
		printf( "%d/%d\n", good, total );
		exit(0);
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
	datalog = malloc( capsize = 1024*1024*8 );
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
