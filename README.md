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

