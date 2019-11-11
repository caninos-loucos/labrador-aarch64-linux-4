# labrador-linux-64
Linux 4 for Caninos Labrador V3

@What is all about?
>This repository contains the source code of Labrador's 64bits linux kernel (version 4.19.37).

>Boards newer than LABRADOR CORE v3.0 should work fine with this kernel!

@What is needed?
>Some tools are necessary for compilation, please read the 'install' file in this repository.

@How to compile?
>Clone the git repository, go to the main path and run make from your favourite terminal.

$ make

@How can I execute an "incremental" build?
>Just execute the command below;

$ make kernel

@How to choose what modules are compiled in?
>Just execute the command below;

$ make menuconfig

@How to transfer the compiled kernel to my board?

>After a successful build, the kernel should be avaliable at output folder.

>The modules are located at output/lib/modules. Copy them to the folder /lib/modules at your Linux system's root.

$ sudo rm -r /lib/modules
$ sudo cp -r output/lib/modules /lib/modules

>Copy the Image file to /boot/ folder.

$ sudo rm /boot/Image
$ cp output/Image /boot/Image

>Copy device tree files to /boot/ folder.

$ rm /boot/v3emmc.dtb
$ rm /boot/v3sdc.dtb
$ cp output/v3emmc.dtb /boot/v3emmc.dtb
$ cp output/v3sdc.dtb /boot/v3sdc.dtb

@How update the Bootloader?

>To update in SD card or a mounted image use;

$ sudo dd if=bootloader.bin of=/dev/*DEVNAME* conv=notrunc seek=1 bs=512
>*DEVNAME* is where your sd card or disk image is mounted on system. You can use lsblk, for example, to know yours.

>To update in a live linux system use;

$ sudo dd if=bootloader.bin of=/dev/mmcblk2 conv=notrunc seek=1 bs=512

@where to find out more about Caninos Loucos and Labrador?

>Access the forum https://forum.caninosloucos.org.br/

>Access our site https://caninosloucos.org/pt/

