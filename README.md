# fx3fun
Charles' playground for the Cypress FX3.  Specifically I'm interested in the CYUSB3011-BZXC/CYUSB3012. 

I'm trying to turn it into a logic analyzer.  I've been playing with a kit, specifically the $45 http://www.cypress.com/documentation/development-kitsboards/cyusb3kit-003-ez-usb-fx3-superspeed-explorer-kit - and I've found it to be incredibly powerful.  This project is split up into a few components.

(1) The Firmware: It is firmware that turns the GPIF-II bus into a 16-bit, 100 MHz sampling device.  Synchronously receiving data and storing it in a buffer to stream back to a computer.  My firmware sets up the isochronous pipes in such a way to transfer data back at about 250 MB/s.  Also, control messages are really easy to send, so this could also be a really useful tool for programming chips, too!  Currently, I'm still using the Windows tools to compile, but shouldn't be hard to use GCC ARM to compile in Linux, especially with the Official Cypress SDK.

(2) CyprIO (pronounced saɪˈpirˈēˈō): My C-library for use in Windows (with CyUSB32.sys) or Linux (with libusb) that lets you talk to my firmware on the cpress chip.  Originally Cypress offered a CyAPI that let you use C++ to talk to their parts, but in digging into it, I found that it was extremely overbearing, and just not a good fit for many of the projects I wanted.  Speicifically, more library-style operations, i.e. introducing into sigrok, etc.  So, I re-wrote it, in C.  Also, a side benefit of this is you can compile it and use it in Windows with only TCC (a 6MB package).  All header files needed to compile are included.  No Visual Studio needed or anything.

The PC-side software works in Windows (TCC) or Linux (GCC). Test in both as of Dec 31, 2017.  Who knows if it'll keep working.

(3) isotest - make control transfer to start sampling GPIF bus and receives data coming back. (Uses CyprIO)

(4) cyprflash - tool that uses CyprIO to boot images on the FX3, and potentially flash a tool that allows writing to an I2C EEPROM on-board, using the official cypress flasher firmware.  NOTE: Shouldn't be hard to support an SPI EEPROM, but I don't have a board with SPI EEPROM to test against.

[![Cypress FX3 as a Possible Logic Analyzer](https://img.youtube.com/vi/_LnZrXrdC00/0.jpg)](https://www.youtube.com/watch?v=_LnZrXrdC00)


# Brief installation notes.

 * git clone this repo somewhere.
 * Download they Cypress EZ-USB FX3 SDK for Linux from here: https://www.cypress.com/documentation/software-and-drivers/ez-usb-fx3-software-development-kit
 * You will find "fx3_firmware_linux.tar.gz" and you can take that and drop "cyfx3sdk" in the same folder that "fx3fun" is located in, so they are parallel.
 * Make a folder in your home folder called `Cypress`
 * Unzip, from the SDK the following folders into `Cypress`
   * `arm-2013.11`
   * `cyusb_linux_1.0.5`
   * `eclipse` * I don't think this is required.

Add the following to your .bashrc

```
export PATH=$PATH:$HOME/Cypress/arm-2013.11/bin
export FX3_INSTALL_PATH=$HOME/Cypress/cyfx3sdk
export ARMGCC_INSTALL_PATH=$HOME/Cypress/arm-2013.11
export ARMGCC_VERSION=4.8.1
export CYUSB_ROOT=$HOME/Cypress/cyusb_linux_1.0.4
```
 * `. ~/.bashrc`
 * Optional if you want to build the firmware:
   * `cd fx3fun/testproject/USBIsoSource`
   * `make`
   * cp ?????? TODO: How to do firmware? BIG TODO: No, really how do you actually make the firmware?  
   * On Windows it looks like it uses `elf2img.exe`
   * `cd ../../`
 * `cd cyprflash`
 * `make`
 * Hold the PMODE jumper shorted.
 * `sudo ./cyprflash  -i -f streaming_image.img`
 * `cd ..`
 * Reboot board, make sure it's up:
   * `lsusb -vv -d 04b4:00f1`
 * ISO Test
   * `cd isotest`
   * 
   

# Licensing

Though everything I am writing may be licensed under the MIT/x11 license OR the NewBSD license (Both of which are compaible with the GPL/LGPL/AFL), there remain some questions about compiling against the Cypress API.  I am still looking into this but hope to clear up questions regarding this soon.

## Project Log

Dec 25, 2017:

I got a Windows 10 PC for once in my life, and decided to dust off the Cypress FX3 Devkit I got two years ago.

Reading Appnote about getting started: http://www.cypress.com/documentation/application-notes/an75705-getting-started-ez-usb-fx3 - specifically the PDF http://www.cypress.com/file/139296/download

Install the: http://www.cypress.com/documentation/software-and-drivers/ez-usb-fx3-software-development-kit

Oh wait... it supports Linux (Rebooting to Linux)

Ok, installing "Download EZ-USB FX3 SDK v1.3.3 for Linux" ... WOW 413 MB is kinda big for something as tiny as I'd expect this to be.

```sudo apt-get install lib32z1 qt4-qmake qt4-dev-tools qt4-linguist-tools qt-sdk libusb-1.0-0-dev eclipse-platform```

Follow installion procedure in http://www.cypress.com/system/files/document/files/FX3_SDK_Linux_Support_0.pdf

Untar contents of all .tar.gz's embedded in the main .tar.gz downloaded to ~/Cypress

Add to .bashrc, the following:

```
export PATH=$PATH:$HOME/Cypress/arm-2013.11/bin
export FX3_INSTALL_PATH=$HOME/Cypress/cyfx3sdk
export ARMGCC_INSTALL_PATH=$HOME/Cypress/arm-2013.11
export ARMGCC_VERSION=4.8.1
export CYUSB_ROOT=$HOME/Cypress/cyusb_linux_1.0.4
```

Open new terminal.
```
cd $FX3_INSTALL_PATH/util/elf2img
gcc elf2img.c -o elf2img -Wall
```

Then
```
cd ~/Cypress/cyusb_linux_1.0.4
make
sudo ./install.sh
```


Now, I'm reading ```~/Cypress/cyfx3sdk/doc/firmware/GettingStartedWithFX3SDK.pdf```

Err... then I noticed https://community.cypress.com/thread/18526 ... say no Linux support for the GPIF-II designer (Which is a critical component) why on earth would they bother releasing a Linux ...

They recommend mono?  



BACK ON TRACK

Go get this thing...  (I may not use it?)

VVV Note added later... I don't think the thing here is useful at all.
```
cd ~/Cypress
git clone https://github.com/nickdademo/cyfwstorprog_linux
make
```

```
cd ~/Cypress/cyusb_linux_1.0.4/bin
sudo ./cyusb_linux
```

(Make sure it works)

Open the SDK.

```
cd ~/Cypress/eclipse
./ezUsbSuite
```

New workspace in: /home/cnlohr/Cypress/test_workspace

Ok, the GPIF button is greyed out.  This is lame.  Switch back to Windows.

In Windows, get and install... http://www.cypress.com/file/139276 ... COMPLETE Installation.

Run C:\Program Files (x86)\Cypress\EZ-USB FX3 SDK\1.3\Eclipse

Make project at C:\projects\fx3fun\testproject

Ok.  Seriously.  Why is the GPIF Designer button still grey.  I gotta figure out how to build a simple project.  I've been at this for almost an hour.

Back to the getting started tab. http://www.cypress.com/file/139296/download

Create a new project with the eclipse thing at C:\projects\fx3fun\testproject ... 

File -> Import... "General" "Existing Projects" C:\Program Files (x86)\Cypress\EZ-USB FX3 SDK\1.3\firmware\gpif_examples\cyfxsrammaster + MAKE SURE TO click "copy project into workspace"

Build release version.

C:\projects\fx3fun\testproject\SRAMMaster\Release\SRAMMaster.img now exists.

Run C:\Program Files (x86)\Cypress\EZ-USB FX3 SDK\1.3\bin\CyControl.exe

Make sure J4 and J3 are on. Burn to ram the image file.

Maybe that worked?

Looks like it _probably_ did, however, I think I want a debugger so I can get printf messages.

How do I do this debug thing.  Connecting to COM5 with 115,200 doesn't show anything, even on the debug firmware, though it seems to with the defualt firmware.

I added a CyU3PDebugPrint command.  Works like a charm..

```
static void
SRAMAppThread_Entry (
        uint32_t input)
{
    /* Initialize the debug module */
    CyFxSRAMApplnDebugInit ();
    CyU3PDebugPrint (CY_FX_DEBUG_PRIORITY, "Debug Test\n" );
```

I SEE IT!!!

Ok.  We're cruising now.

Got it.  The cyfxsrammaster project was cool... But, let's first see if we can actually talk to an app via USB 3.0.  Maybe some sort of speed test to make sure we can sustain the needed speeds?

Delete that project.

Try File->Import... General/Existing "C:\Program Files (x86)\Cypress\EZ-USB FX3 SDK\1.3\firmware\basic_examples\cyfxisosrc" I want an isochronous source! (Don't forget to check the copy workspace box.)

Build and flash (in debug)

Fire up "streamer.exe" from C:\Program Files (x86)\Cypress\EZ-USB FX3 SDK\1.3\bin

Osnap.  It works.  Fast.  My computer seems to handle 256MB/s pretty well.  Our target is 200MB/s, so I'd say we're doing pretty good.

Their app is all C++'d up... So, I am looking through the code for it as well as this... https://github.com/Cornell-RPAL/occam/blob/master/indigosdk-2.0.15/third/cyusb/CyAPI.cpp

Their code is called with ```USBDevice = new CCyUSBDevice((HANDLE)this->Handle,CYUSBDRV_GUID,true);```

Yep... totally ripping it off... slowly.  Spent 2 hours so far... Now, I can at least get a handle to the device.  Still need to start sending it control messages to set up the isochronous endpoint.

Maybe tomorrow?

Then, who knows!  

12/26/2017:

Spent another hour working away with my teststream.c thing... Then started to wonder if libUSB could be leveraged...  Tempting...

I'll keep chugging away for a bit, I guess.

Still going after about 6 hours.  Time to take a break.  Was mostly just converting over code to CyprIO.

NEAT! The very first time after it compiled, it ran!  Committing.

Only getting 12 MB/sec, though.  I think it's cause I'm creating and deleting all my transfers.

TODO: Where are we doing the equivelent of                     if (USBDevice->SetAltIntfc(i) == true )?

AH! Looking at Streamer.h they do their own custom function to get data that queues up a bunch of buffers.

We shall do the same.

Man, I'm glad I didn't give up and go another route right now.

Well, computer just did the Win10 equivelent of bluescreening and I lost one of my source files.  Thankfully it only took ~15 mins to edit from the last git commit.

Gotta start git committing more frequently.

AWW MAN YEAAAAAAAA.  Turns out was an uninitialized value.  P.S. Neat article.  Need to be careful about the overlapped structures.  https://blogs.msdn.microsoft.com/oldnewthing/20110202-00/?p=11613 Thanks https://twitter.com/cnlohr/status/945907823560613888 @Mr_Red_Beard

So, now, if you open the Cypress app and select the second option for the data flow, you get 256,000 kB/s via both apps.

Seriously... how does SetAltIntfc work?  It does seem to set some configuration the device...  Found it. 	return CyprIOControl(ths, IOCTL_ADAPT_SELECT_INTERFACE, &alt, 1L);

Works like a charm.

Now to figure out why the data's all wrong.  I was just being dumb.  Data went where I wanted it.

Ok, got rid of a ton of stuff from Cypress.  Still more to get rid of.  But man this is curising along well.  About 1% or less CPU utilization at 256 MB/sec.

MOVING OVER TO THE USB SIDE

The GPIF designer is pretty easy to use.  Took a bit to get the hang of it, but I think I can make something with a data buffer, and one state that just does a ton of data in.

12/27/2017:

Spend 20 minutes dinking more... I think it's time to ask for help and go do regular work.

https://community.cypress.com/message/148992#148992

Well, got that working.  Was a simple buffer problem.  

I'm gonna need a way of selecting between different run modes, maybe slower modes, too.  Better get control messages going.  libusb did it right.  We should mimic them.

This is totally kicking my butt.  My function somehow differs from theirs.

Some sort of strange stack corruption.  Reading/writing values on the stack seems hosed.  Need to understand more.

INCREDIBLE!!! So, if you start sending data to an iso port and then disconnect, if the data is still sending from the GPIF bus, and you don't send to it. 

Video time!

Ok, video didn't go well.

2 AM 12/29/2017: Trying out different sizes of DMA, etc.  Looks like it works in chunks of 2048 just fine but still overflows incessently.

Post reply to https://community.cypress.com/thread/32169

Dinked around for about an hour, got nowhere with the bandwidth, though I did notice I can overclock my bus to 8/7ths of 100 MHz :-D

Gave up around 8 AM...

12/29 2:00 PM Started again.  Had to use a watermark or fifo-space-left operation with extra states, but it worked!  (2 hours more)

Started again at 10:45 PM.  How hard is it to flash custom firmware?  Or maybe I should cleanup Cyprio?

They provide "c_sharp/controlcenter/Form1.cs" which is 3,000 lines long.  :(.

(only spent 15 minutes this time)

12/30 Midnight: Starting again.

Just found out all of the cypress stuff goes through CyUSB3.sys. http://www.cypress.com/file/145261/download
Looks like it all gets piped through a kernel mode driver I got, so it turns out it does take a driver :( -- but apparently it's an easy-for-windows-to-deal-with driver!!!

Spent about 2 hours looking at the hex of the cypress images and a USBPcap dump of a successful flash of the device using the Cypress tool.  That should have taken way less time.

Now, to figure out why my messages I think are CONTROL OUT messages are turning into CONTROL IN messages >.<

Got it to boot!!! (RAM only, no writing to flash!)

NEXT: How do we flash to the I2C EEPROM? (3:00 AM)

Apparently, that's easy.  First, RAM it to the Flasher firmware.  Then call flash commands. (0x40/0xBA/(Windex/Wvalue) of the raw binary image.  No interpretation needed.

AND NOW IT WORKS WITH I2C EEPROM (3:50 AM)

Dec 30, starting around 8 PM... Porting to Linux.

Just mostly #define out the stuff that is flagrantly Windows.

Booted back into Windows to make sure it was still working.  It wasn't.  Also, while in Windows, ditched any of the CyUSB3 stuff in critical path.  It could be replaced by generic calls to the control transfer stuff.

WOOHH SUCCESSFULLY FLASHING IN LINUX!!!  (11:08 PM!)

Woah this isochronous thing was kicking my butt.  12:54 AM, Dec 31, 2017.  Iso in linux looks weird, but seems to be working.  I'm worried it may be dropping packets.

Back and forth to Windows once more and everything works in both.

Dec 31, 1:09 AM.  Remerging back into master.
