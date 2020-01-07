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

#include <CNFGFunctions.h>

void * TickThread( void * v );

double samples_per_second;

struct CyprIO eps;

#define BUFFERSIZE_SAMPLES (1024*1024)
#define MAX_BUFFERS 2

int TriggerDirection;
int Triggering;
int DisableMask;
int WatchMask = 2;

struct SetBuffer
{
	uint16_t Data[BUFFERSIZE_SAMPLES];
	uint32_t head;
	int32_t trigger_point;
};

int RunningBuffer;
int DisplayBuffer;
struct SetBuffer Buffers[MAX_BUFFERS];
uint16_t LastSample;
int hello;

int Record16;
FILE * RecordFile;
int RecordSize;
int Recording;
char RecordFileName[128];


int callback( void * id, struct CyprIOEndpoint * ep, uint8_t * data, uint32_t length )
{
	uint16_t * datas = (uint16_t*)data;
	static double Last;
	static int bytes;
	int i;
	length /= 2;
	bytes += length;
	double Now = OGGetAbsoluteTime();

	int TriggerMask = DisableMask?0:WatchMask;  //Actually used

	if( Recording )
	{
		if( Record16 )
		{
			fwrite( data, length*2, 1, RecordFile );
			RecordSize += length * 2;
		}
		else
		{
			uint8_t latch[length];
			for( i = 0; i < length;i ++ )
			{
				latch[i] = datas[i];
			}
			fwrite( latch, length, 1, RecordFile );
			RecordSize += length;
		}
	}

#if 1
	struct SetBuffer * sb = &Buffers[RunningBuffer];
	for( i = 0; i < length; i++ )
	{
		uint16_t sample = datas[i];
		sb->Data[sb->head] = sample;
		sb->head = (sb->head+1)&(BUFFERSIZE_SAMPLES-1);
		if( sb->trigger_point > -1 )
		{
			if( ((sb->trigger_point + (BUFFERSIZE_SAMPLES/2)) % BUFFERSIZE_SAMPLES) == sb->head )
			{
				//Switch buffers.
				DisplayBuffer = RunningBuffer;
				RunningBuffer = ( RunningBuffer + 1 ) % MAX_BUFFERS;
				sb = &Buffers[RunningBuffer];
				sb->trigger_point = -1;
				Triggering = 0;
			}
		}
		else
		{
			//Check to see if we need to trigger.
			if( TriggerMask && !Triggering )
			{
				if( TriggerDirection ) sample = ~sample;
				if( !(sample & TriggerMask) )
				{
					Triggering = 1;
					sb->trigger_point = sb->head;
				}
			}
		}
	}
#endif

	if( Last + .2 < Now )
	{
		if( Last == 0 ) Last = Now;
		//printf( "Got %.3f kB/s [%02x %02x] [%d pack]\n", bytes/1000, data[0], data[1], length );
		samples_per_second = bytes * 5;
		Last+=0.2;
		bytes = 0;
	}

	LastSample = datas[length-1];

	return 0;
}

void * CircularRxThread( void * v )
{
#if defined( WINDOWS ) || defined( WIN32)
	CyprIODoCircularDataXferTx( &eps.CypIOEndpoints[0], 65536*16, 8,  callback, 0 );
#else
	CyprIODoCircularDataXferTx( &eps.CypIOEndpoints[0], 32768, 8,  callback, 0 );
#endif
}

#if !defined(WINDOWS) && !defined(WIN32)
void ExitApp()
{
	eps.shutdown = 1;
	usleep(100000);
	CyprIODestroy( &eps );
}
#endif

void DrawFrame()
{
	double Now = OGGetAbsoluteTime();
	short w, h;
	CNFGGetDimensions( &w, &h );
	char stt[1024];

	struct SetBuffer * sb = &Buffers[DisplayBuffer];


	CNFGBGColor = 0x400000;
	CNFGHandleInput();
	CNFGClearFrame();
	CNFGColor( 0xffffff );

	CNFGPenX = 10;
	CNFGPenY = 10;
	sprintf( stt, "%5.0f msps  %04x [%d]", 	samples_per_second/1000000, LastSample, DisplayBuffer );
	CNFGSetLineWidth( 2 );
	CNFGDrawText( stt, 3 );


	if( Recording )
	{
		CNFGPenX = 10;
		CNFGPenY = 30;
		sprintf( stt, "%5.0f MBytes %s", RecordSize/(1024*1024.0), RecordFileName );
		CNFGSetLineWidth( 2 );
		CNFGDrawText( stt, 3 );
	}


	CNFGSetLineWidth( 1 );

	int i;
	for( i = 0; i < 16; i++ )
	{
		int mask = 1<<i;
		CNFGDialogColor = (LastSample&mask)?0x0000ff:0x000000;
		int sy = 5;
		int sz = 20;
		int sx = w - sz * 16 - sy;
		CNFGDrawBox( sx + i * sz + 2,  sy, sx + i * sz + sz, sy+sz );
		CNFGPenX = sx + i * sz + 2 + 6;
		CNFGPenY = sy + 3;
		sprintf( stt, "%x", i );
		CNFGDrawText( stt, 3 );
	}

	int channel;
	int channels = 8;
	for( channel = 0; channel < channels; channel++ )
	{
		int mask = 1<<channel;

		int lw = w - 100;
		int fmark = (sb->trigger_point>=0)?(sb->trigger_point-lw/2+BUFFERSIZE_SAMPLES):(sb->head+BUFFERSIZE_SAMPLES);
		fmark = fmark & (BUFFERSIZE_SAMPLES-1);

		i = 50;

		int my = (sb->Data[fmark]&mask)?1:0;
		int mx = i;
		int chans = 100;
		int chanh = ( h - 100 ) / channels;
		int chanhl = chanh - 20;
		int tmy;

		for( ; i < w-50; i++ )
		{
			tmy = (sb->Data[fmark]&mask)?1:0;

			if( tmy != my )
			{
				CNFGTackSegment( mx, my * chanhl + channel * chanh + chans, i, my * chanhl + channel * chanh + chans );
				CNFGTackSegment( i, my * chanhl + channel * chanh + chans, i, tmy * chanhl + channel * chanh + chans );
				mx = i;
				my = tmy;
			}

			fmark++;
			fmark = fmark & (BUFFERSIZE_SAMPLES-1);
		}
		CNFGTackSegment( mx, my * chanhl + channel * chanh + chans, i, tmy * chanhl + channel * chanh + chans );
	}

	void DrawButtons();
	DrawButtons();

	CNFGSwapBuffers();


	//Only allow operation at ~60 Hz.
	static double LastTime;
	double NewNow = OGGetAbsoluteTime();
	double dlay = LastTime + 0.017 - NewNow;
	if( dlay > 0 )
	{
		usleep( dlay*1000000 );
	}
	if( dlay < 0 )
	{
		LastTime = Now;
	}
	else
	{
		LastTime += 0.017;
	}
	
}

int main()
{
	signal(SIGINT, ExitApp);

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

	//Set up scope stuff...
	Buffers[0].trigger_point = -1;



	CNFGSetup( "fx3view", 800, 600 ); //return 0 if ok.

	
	printf( "Connected successfully\n" );
		

	OGCreateThread( TickThread, 0 );
	OGCreateThread( CircularRxThread, 0 );

	printf( "...\n" );


	while( !eps.shutdown )
	{
		DrawFrame();
	}

	
	return 0;
}



struct Button;
struct Button
{
	int x, y, w, h, id;
	int (*BtnClick)( struct Button * );
	const char * text;
	uint32_t bgcolor;
	float focus;
};




int Pause( struct Button * btn )
{
	btn->bgcolor = 0x0000ff;
	btn->id = !btn->id;
	DisableMask = btn->id;
	return 0;
}



int RecordFileButton( struct Button * btn )
{
	if( Recording )
	{
		Recording = 0;
		fclose( RecordFile );
		printf( "Done writing file.\n" );
		if( btn->id ) btn->text = "RECORD 16"; else btn->text = "RECORD 8";
	}
	else
	{
		Record16 = btn->id;
		time_t rawtime;
		struct tm *info;
		time( &rawtime );
		info = localtime( &rawtime );
		if( Record16 ) 
			strftime(RecordFileName,80,"Data_%Y%m%d%H%M%S.16.dat", info);
		else
			strftime(RecordFileName,80,"Data_%Y%m%d%H%M%S.8.dat", info);
		printf( "Writing to %s\n", RecordFileName );
		RecordFile = fopen( RecordFileName, "wb" );
		RecordSize = 0;
		Recording = 1;
		btn->text = "RECORDING";
	}
	return 0;
}



struct Button gbtns[] = {
	{ .x = 20, .y = 50, .w = 60, .h = 20, .id = 0, .BtnClick = &Pause, .text = "PAUSE", .bgcolor = 0x202020 },
	{ .x = 100, .y = 50, .w = 180, .h = 20, .id = 0, .BtnClick = &RecordFileButton, .text = "RECORD 8", .bgcolor = 0x202020 },
	{ .x = 300, .y = 50, .w = 180, .h = 20, .id = 1, .BtnClick = &RecordFileButton, .text = "RECORD 16", .bgcolor = 0x202020 },
};

int cursorx, cursory;
int downmask;
int downbutton = -1;

void DrawButtons()
{
	int buttons = sizeof( gbtns ) / sizeof( gbtns[0] );
	int i;

	int x = cursorx;
	int y = cursory;
	for( i = 0; i < buttons; i++ )
	{
		struct Button * b = &gbtns[i];
		if( x >= b->x && y >= b->y && x <= b->x + b->w && y <= b->y + b->h )
		{
			b->focus += (1.0 - b->focus) * .1;
		}
	}



	CNFGColor( 0xffffff );
	for( i = 0; i < buttons; i++ )
	{
		struct Button * b = &gbtns[i];
		uint32_t rb = b->bgcolor;
		uint32_t re = rb & 0xff;
		uint32_t ge = (rb >> 8) & 0xff;
		uint32_t be = (rb >> 16) & 0xff;
		re = b->focus * 200;
		ge = b->focus * 200;
		be = b->focus * 200;
		if( re > 255 ) re = 255;
		if( ge > 255 ) ge = 255;
		if( be > 255 ) be = 255;
		CNFGDialogColor = re | (ge<<8) | (be<<16);
		b->focus -= 0.04;
		if( b->focus < 0 ) b->focus = 0;

		CNFGDrawBox( b->x, b->y, b->x+b->w, b->y+b->h );
		CNFGPenX = b->x + 4;
		CNFGPenY = b->y + 4;
		CNFGDrawText( b->text, 3 );
	}
}

void HandleKey( int keycode, int bDown )
{
}

void HandleButton( int x, int y, int button, int bDown )
{
	int buttons = sizeof( gbtns ) / sizeof( gbtns[0] );
	int i;
	cursorx = x;
	cursory = y;
	if( bDown )
		downmask |= 1<<button;
	else
		downmask &=~(1<<button);

	for( i = 0; i < buttons; i++ )
	{
		struct Button * b = &gbtns[i];
		if( x >= b->x && y >= b->y && x <= b->x + b->w && y <= b->y + b->h )
		{
			int down = !(downmask & 1);
			if( down )
			{
				downbutton = i;
			}
			else
			{
				if( downbutton == i )
				{
					b->BtnClick( b );
					b->focus = 2;
				}
			}
		}
	}
}

void HandleMotion( int x, int y, int mask )
{
	cursorx = x;
	cursory = y;
	downmask = mask;
}

void HandleDestroy()
{
}




void TickCypr()
{
	char buf[64];
	int e = CyprIOControlTransfer( &eps, 0xc0, 0xaa, 0x0102, 0x0304, buf, 50, 10 );
}

void * TickThread( void * v )
{
	while(1)
	{
		TickCypr();
		OGSleep(1);
	}
}
