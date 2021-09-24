/*
	Tool to boot Cypress FX3 to image file, or flash image file to external i2c EEPROM.	
	(C) 2017 C. Lohr, under the MIT-x11 or NewBSD License.  You decide.
	Tested on Windows, working full functionality 12/30/2017
	Tested on Linux, working full functionality 12/31/2017
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//For 'sleep' / 'usleep'
#if defined( WIN32 ) || defined( WINDOWS )
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <libcyprio.h>


int main( int argc, char ** argv )
{
	int do_flash_i2c = 0;
	int desired_pid = 0x4720;
	int dont_use_bootloader = 0;
	const char * file = 0;
	int i;
	int showhelp = 0;
	int do_reboot = 1;
	
	for( i = 1; i < argc; i++ )
	{
		const char * proc = argv[i];
		if( *proc == '-' )
		{
			do
			{
				proc++;
				if( !*proc ) continue;
				if( *proc == 'i' )
				{
					do_flash_i2c = 1;
				}
				else if( *proc == 'c' )
				{
					dont_use_bootloader = 1;
				}
				else if( *proc == 'f' )
				{
					file = argv[i+1];
					i++;
				}
				else if( *proc == 'n' )
				{
					do_reboot = 0;
				}
				else if( *proc == 'p' )
				{
					desired_pid = atoi( argv[i+1] );
					i++;
				}
				else
				{
					showhelp = 1;
					break;
				}
			} while( *proc );
		}
		else
		{
			showhelp = 1;
		}
		if( showhelp ) break;
	}
		
	if( showhelp || file == 0 || file[0] == 0 )
	{
		fprintf(stderr, "Usage: %s [-chi] [-p pid] [-f file...]\n", argv[0]);
		fprintf(stderr, " -c: Don't use bootload (your app must be able to write flash!)\n");
		fprintf(stderr, " -h: Show help\n");
		fprintf(stderr, " -p: use specific PID\n");
		fprintf(stderr, " -i: Use i2c flash\n");
		fprintf(stderr, " -n: do not reboot\n");
		exit(-1);
	}
	
	if( do_flash_i2c )
	{
		FILE * firmware = fopen( file, "rb" );
		if( !firmware || ferror(firmware) )
		{
			fprintf( stderr, "Error: can't open file \"%s\" for permanant write.\n", file );
			return -5;
		}
		
		if( !dont_use_bootloader )
		{
			int r = CyprIOBootloaderImage( "CyBootProgrammer.img" );
			if( r )
			{
				printf("Failed to flash bootloader.\n");
			}
		}
		
		//Bootloader OK.  Need to flash binary.
		
		#if defined( WIN32 ) || defined( WINDOWS )
		Sleep(1000);
		#else
		sleep(1);
		#endif
		
		struct CyprIO eps;
		
		int connected = 0;
		if( CyprIOConnect( &eps, 0, 0x04b4, desired_pid ) )
		{
			fprintf( stderr,"Could not connect to flasher.\n" );
			exit( -5 );
		}

		printf( "Re-enumeration complete.\n" );
		int flashspot = 0;
		char buff[2048];
		while( !feof(firmware) && !ferror( firmware ) )
		{
			int r = fread( buff, 1, 2048, firmware );
			if( r < 0 )
			{
				fprintf( stderr, "Unexpected end of firmware read\n" );
				break;
			}
			int tries;
			for( tries = 0; tries < 10; tries++ )
			{
				int e = CyprIOControlTransfer( &eps, 0x40, 0xbc, flashspot>>16, flashspot, buff, r, 5000 );
				if( e == r )
				{
					char verify[2048];
					e = CyprIOControlTransfer( &eps, 0xc0, 0xbc, flashspot>>16, flashspot, verify, r, 5000 );
					int i;
					#if 0
					for( i =0 ;i < r; i++ )
					{
						if( !(i&15) ) printf( "\n%04x ", i );
						printf( "%02x %02x // ", (uint8_t)verify[i], (uint8_t)buff[i] );
					}
					printf( "\n" );
					printf( "%02x == %02x   %02x == %02x\n", buff[0], verify[0], buff[256], verify[256] );
					#endif
					if( e == r )
					{
						if( memcmp( verify, buff, r ) == 0 )
							break;
						else
						{
							fprintf( stderr, "WARNING: Verify match failed.\n" );
							continue;
						}
					}
				}
				fprintf( stderr, "WARNING: Had to re-attempt write at address %08x\n", flashspot );
			}
			if( tries == 10 )
			{
				fprintf( stderr, "ERROR: Too many retries.  Aborting.\n" );
				CyprIODestroy( &eps );

			}
			printf( "." );
			fflush ( stdout );
			flashspot += r;
		}

		if( do_reboot )
		{
			int e = CyprIOControlTransfer( &eps, 0x40, 0xbc, 0, 0, buff, 0, 5000 );
			if( e )
			{
				fprintf( stderr, "WARNING: Could not issue finalization\n" );
			}
		}
		printf( "Flash complete.\n" );
		CyprIODestroy( &eps );
	}
	else
	{
		int r = CyprIOBootloaderImage( file );
		if( !r )
		{
			printf( "Flash succeeded.\n" );
			return 0;
		}
		else
		{
			printf( "Flash failed.\n" );
			return -5;
		}
	}
}

