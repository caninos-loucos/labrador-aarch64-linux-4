# labrador-linux-64
Linux 4 kernel source code for "Caninos Labrador v3.0".

## Release Notes
Current (12-05-2020)
1) Kernel version updated from 4.19.37 to 4.19.98
2) Added base board user configurable led support
3) New fully functional and complete caninos-clk driver (written from scratch)
4) Caninos KMS/DRM driver prepared for hdmi custom video modes and cvbs output
5) Corrected a plethora of minor bugs

## About
This repository contains the source code of Caninos Labrador's 64bits linux
kernel.
Boards newer than "Labrador Core v3.0" should work fine with this kernel.

## Usage
Some tools are necessary for compilation, please read the "install" file in this
repository.
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
3) To compile your kernel image
```
make kernel
```
4) To reset everything
```
make clean
```

## Installation
After a successful build, the kernel should be avaliable at "output" folder.
The modules are located at "output/lib/modules". Do the following:

1) Copy the modules to the folder "/lib/modules" at your SDCARD/EMMC system's
root.

```
$ sudo cp -r output/lib/modules $ROOTFS/lib/
```

2) Copy the Image file to the "/boot/" folder at your SDCARD/EMMC system's root.

```
$ sudo cp -r output/Image $ROOTFS/boot/
```

3) Copy the device tree files to "/boot/" folder at your SDCARD/EMMC
system's root.

```
$ sudo cp -r output/v3emmc.dtb $ROOTFS/boot/
$ sudo cp -r output/v3sdc.dtb $ROOTFS/boot/
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

Caninos Loucos Forum: <https://forum.caninosloucos.org.br/>

Caninos Loucos Website: <https://caninosloucos.org/>

