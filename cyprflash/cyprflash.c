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
	const char * file = 0;
	int i;
	int showhelp = 0;
	
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
				else if( *proc == 'f' )
				{
					file = argv[i+1];
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
		
	if( showhelp )
	{
		fprintf(stderr, "Usage: %s [-hi] [-f file...]\n", argv[0]);
		exit(-1);
	}
	
	if( file == 0 || file[0] == 0)
	{
		fprintf( stderr, "Error: need at least one file parameter to application\n" );
		exit( -2 );
	}
	
	if( do_flash_i2c )
	{
		FILE * firmware = fopen( file, "rb" );
		if( !firmware || ferror(firmware) )
		{
			fprintf( stderr, "Error: can't open file for permanant write.\n" );
			return -5;
		}
		int r = CyprIOBootloaderImage( "CyBootProgrammer.img" );
		if( r )
		{
			printf("Failed to flash bootloader.\n");
		}
		
		//Bootloader OK.  Need to flash binary.
		
		#if defined( WIN32 ) || defined( WINDOWS )
		Sleep(1000);
		#else
		sleep(1);
		#endif
		
		struct CyprIO eps;
		
		if( CyprIOConnect( &eps, 0, 0x04b4, 0x4720 ) )
		{
			fprintf( stderr,"Could not connect to flasher. TODO: Should this function retry?\n" );
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
				int e = CyprIOControlTransfer( &eps, 0x40, 0xba, flashspot>>16, flashspot, buff, r, 5000 );
				if( e == r )
				{
					char verify[2048];
					e = CyprIOControlTransfer( &eps, 0xc0, 0xbb, flashspot>>16, flashspot, verify, r, 5000 );
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

		int e = CyprIOControlTransfer( &eps, 0x40, 0xbb, 0, 0, buff, 0, 5000 );
		if( e )
		{
			fprintf( stderr, "WARNING: Could not issue finalization\n" );
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

