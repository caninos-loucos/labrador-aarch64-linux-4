# labrador-linux-64
Linux 4 kernel source code for "Caninos Labrador v3.0".

## Release Notes

### Next Release (TBA)
1) Add driver for HDMI audio playback
2) Enable HDMI-CEC functionality at HDMI-IP driver
3) Add driver for TFT LCD displays using DSI interface
4) New driver for Video Decode Engine

### Current Release (26-01-2021)
1) Add driver for GPU power regulator
2) Activated more GPIO pins
3) Enable Mali 450 GPU driver
4) Corrected minor bugs

### Third Release (05-09-2020)
1) Added HDMI screen resolution switching feature in the KMS driver
2) New HDMI-IP driver (HDMI-CEC functionality not activated)
3) New Display Engine driver (only basic 2D functions implemented)
4) Added thermal sensor driver for GPU, CPU and CoreLogic
5) Added driver for user configurable baseboard buttons
6) Added Mali 450 GPU driver
7) Corrected minor bugs

### Second Release (24-07-2020)

1) Added hardware based PWM driver
2) Added dynamic GPIO/Device function muxing at external header
3) New GPIO and Pinctrl drivers (written from scratch to remove deprecated APIs)
4) New Reset driver with better performance
5) Increased PADs drive strength
6) Corrected PMIC issues related to system shutdown/restart
7) General code clean-up and optimization

### First Release (15-05-2020)

1) Kernel version updated from 4.19.37 to 4.19.98
2) Added base board user configurable led support
3) New fully functional and complete caninos-clk driver (written from scratch)
4) Caninos KMS/DRM driver prepared for hdmi custom video modes and cvbs output
5) Added base board 10bit ADC input support
6) Corrected a plethora of minor bugs

## About
This repository contains the source code of Caninos Labrador's 64bits linux
kernel. Boards newer than "Labrador Core v3.0" should work fine with this
kernel.

## Usage
Prior to compilation, make sure you have the following libraries and/or
tools installed on your machine:
1) GNU Binutils environment
2) Native GCC compiler
3) GCC cross-compiler targeting "gcc-aarch64-linux-gnu"
4) Make build tool
5) Git client
6) Bison and Flex development libraries
7) NCurses development libraries
8) LibSSL development libraries

```
$ sudo apt-get update
$ sudo apt-get install build-essential binutils gcc-aarch64-linux-gnu make git
$ sudo apt-get install bison flex libncurses-dev libssl-dev
```

After installing these tools, clone this repository in your computer.
Then, go to it's main directory and execute it's makefile.

```
$ git clone https://github.com/caninos-loucos/labrador-linux-64.git
$ cd labrador-linux-64 && make
```

## Incremental Build
If you want to do an incremental build, execute the following commands:

1) To load "Labrador Core v3.0"'s configuration
```
make config
```
>Note: this will overwrite all configurations already set up.

2) To change which modules are compiled into your kernel image
```
make menuconfig
```
3) To compile the device tree binary blob
```
make dtbs
```
4) To compile your kernel image
```
make kernel
```
5) To reset everything
```
make clean
```

## Kernel Installation
After a successful build, the kernel should be avaliable at "output" folder.
The modules are located at "output/lib/modules". Do the following:

1) Copy the modules to the folder "/lib/modules" at your SDCARD/EMMC system's
root.

```
$ sudo cp -r output/lib/modules $ROOTFS/lib/
```

2) Copy the Image file to the "/boot/" folder at your SDCARD/EMMC system's root.

```
$ sudo cp output/Image $ROOTFS/boot/
```

3) Copy the device tree files to "/boot/" folder at your SDCARD/EMMC
system's root.

```
$ sudo cp output/v3emmc.dtb $ROOTFS/boot/
$ sudo cp output/v3sdc.dtb $ROOTFS/boot/
```
>Note: $ROOTFS must be replaced by the complete directory path of your target
system's root mounting point.

## Bootloader Installation

To update the bootloader in a SDCARD use:
```
$ sudo dd if=bootloader.bin of=/dev/$DEVNAME conv=notrunc seek=1 bs=512
```
> Note: $DEVNAME is the name of the target device. It is not a partition name.
You can use lsblk, to know it's name.

To update the EMMC's bootloader from a live Linux system booted from SDCARD use:
```
$ sudo dd if=bootloader.bin of=/dev/mmcblk2 conv=notrunc seek=1 bs=512
```

## Contributing

**Caninos Loucos Forum: <https://forum.caninosloucos.org.br/>**

**Caninos Loucos Website: <https://caninosloucos.org/>**

