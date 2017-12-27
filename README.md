# fx3fun
Charles' playground for the Cypress FX3.  Specifically I'm interested in the CYUSB3011-BZXC/CYUSB3012. 


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
export IDF_PATH=~/esp/esp-idf
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