/*
	API Routine to boot Cypress FX3 to a specific firmware image.
	(C) 2017 C. Lohr, under the MIT-x11 or NewBSD License.  You decide.
	Tested on Windows, working full functionality 12/30/2017
*/

#include <libcyprio.h>
#include <stdio.h>

int CyprIOBootloaderImage( const char * fwfile )
{
	FILE * firmware = 0;
	struct CyprIO eps;

	int r = CyprIOConnect( &eps, 0, 0x04b4, 0x00f3 );
	if( r )
	{
		fprintf( stderr, "Error: Could not connect to USB device\n" );
		goto error;
	}
	/*
	//Not needed if we're not using pipes/etc.
	r = CyprIOGetDevDescriptorInformation( &eps );
	if( r )
	{
		fprintf( stderr, "Error: Couldn't get all info needed from device\n" );
		goto error;
	}
	r = CyprIOSetup( &eps, 0, 2 );
	if( r )
	{
		fprintf( stderr, "Error: Could not setup USB device\n" );
		goto error;
	}
	*/
	
	
	
	printf( "Connected successfully\n" );
	uint8_t buf[2048];
	uint8_t comp[2048];

	int k = CyprIOControlTransfer( &eps, 0xc0, 0xa0, 0, 0, comp, 1, 5000 );
	if( k != 1 || comp[0] != 0x24 )
	{
		fprintf( stderr, "Error: Control message returned confusing result.\n" );
		goto error;
	}
	
	firmware = fopen( fwfile, "rb" );
	uint8_t header[4];
	fread( header, 4, 1, firmware );

	if( header[0] != 'C' || header[1] != 'Y' )
	{
		fprintf( stderr, "Invalid image file.\n" );
		goto error;
	}
	
	uint32_t flashspot = 0x100;
	uint32_t remain = 0;
	while( !feof( firmware ) && !ferror( firmware ) )
	{
		uint8_t arr[8];
		int r = fread( arr, 1, 8, firmware );
		if( r == 4 )
		{
			//Checksum or something at the end of the file?  Eh, don't care.
			printf( "Flashing done.\n" );
			break;
		}
		if( r != 8 )
		{
			fprintf( stderr, "Can't read header section in firmware at %d (%d)\n", ftell(firmware), r );
			goto error;
		}

		//43 59 1C B0 ||| 8A 08 00 00 00 01 00 00 = Read 2228 bytes.  088a * 4 = 2228
		//   (@2234)      00 40 00 00 00 30 00 40 = Read ???
		flashspot = arr[4] | (arr[5]<<8) | (arr[6]<<16) | (arr[7]<<24);
		remain = arr[0] | (arr[1]<<8) | (arr[2]<<16) | (arr[3]<<24);
		remain *= 4;

		if( remain == 0 )
		{
			int e = CyprIOControlTransfer( &eps, 0x40, 0xa0, flashspot, flashspot>>16, buf, 0, 5000 );
			printf( "Special operation with size zero return value: %d\n", e );
			continue;
		}
		
		printf( "Flashing section %08x with %d bytes (@%08x)\n", flashspot, remain, ftell (firmware ) );
		
		while( !feof( firmware ) && !ferror( firmware ) && remain > 0 )
		{
			int toread = (remain<2048)?remain:2048;
			int r = fread( buf, toread, 1, firmware );
			if( r <= 0 )
			{
				fprintf( stderr, "Error reading firmware file at address %d\n", ftell(firmware) );
				goto error;
			}
			remain -= toread;
			int tries = 0;
			
			do
			{
				int e = CyprIOControlTransfer( &eps, 0x40, 0xa0, flashspot, flashspot>>16, buf, toread, 5000 );
				int k = CyprIOControlTransfer( &eps, 0xc0, 0xa0, flashspot, flashspot>>16, comp, toread, 5000 );
				flashspot += toread;
				if( e != k || e != toread || memcmp( comp, buf, toread ) != 0 )
				{
					fprintf( stderr, "Retrying... (%d/%d/d)", e, k, toread );
					if( tries++ > 10 )
					{
						fprintf( stderr, "Error: Can't flash.\n" );
						goto error;
					}
				}
				else
				{
					break;
				}
			} while( 1 );
		}
	}
	CyprIODestroy( &eps );
	return 0;
error:
	if( firmware ) fclose( firmware );
	CyprIODestroy( &eps );
	return -1;
}
