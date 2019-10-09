# labrador-linux-64
Linux 4 for Caninos Labrador V3

@What is about?
>This repository has 64bits linux kernel for Labrador, actually working with kernel 4.19.37.

>Boards newlest than LABRADOR CORE v3.0 should work fine with the kernel.

@What is needed?
>To be properly compiled, could be necessary some tools, please read the 'install' file in this repository.

@How to compile?
>To compile this Kernel, once that you are in this path, just run make from terminal.

$ make

@How to do an incremental compilation?
>To incremental compilation just run from terminal;

$ make kernel

@How to transfer the compiled kernel to the board?

>If the compilation is complete with success, the compiled kernel should be avaliable at path /build/

>Update the modules in Labrador;

$ sudo rm -r /lib/modules
$ sudo cp -r /src/build/lib/modules /lib/

>Update the Image file in Labrador

$ sudo rm /media/caninos/FA82-D061/Image
$ cp /src/build/Image /media/caninos/FA82-D061/

>Update device tree files in Labrador

$ rm /media/caninos/FA82-D061/v3emmc.dtb
$ rm /media/caninos/FA82-D061/v3sdc.dtb
$ cp /src/build/v3emmc.dtb /media/caninos/FA82-D061/
$ cp /src/build/v3sdc.dtb /media/caninos/FA82-D061/

*/src/ is always the path where the folder or files are.

@How change/update the Bootloader?

>To change in SD card use;

$ sudo dd if=/src/bootloader.bin of=/dev/*SDCARD* conv=notrunc seek=1 bs=512
>*SDCARD* is where your sd card is mounted on system, use lsblk for example to know yours.

>To change in Labrador use;

$ sudo dd if=/src/bootloader.bin of=/dev/mmcblk2 conv=notrunc seek=1 bs=512


>*/src/ is always the path where the folder or files are.

@where find more about Caninos Loucos and Labrador?

>Access the forum https://forum.caninosloucos.org.br/
>Access our site https://caninosloucos.org/pt/



