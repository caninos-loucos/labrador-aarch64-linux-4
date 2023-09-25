#!/bin/sh
USER=$(whoami)

if [ -d /media/BOOT ]; then
	echo "BOOT mounted in /media"
	BOOTDIR=/media/BOOT/
elif [ -d /media/${USER}/BOOT ]; then
	echo "BOOT mounted in /media/user"
	BOOTDIR=/media/${USER}/BOOT/
elif [ -d /run/media/${USER}/BOOT ]; then
	echo "BOOT mounted in /run/media/user"
	BOOTDIR=/run/media/${USER}/BOOT/
else
	echo "BOOT directory not found or mounted, quitting."
	exit
fi 

if [ -d /media/SYSTEM ]; then
	echo "SYSTEM mounted in /media"
	LIBDIR=/media/SYSTEM/lib/
elif [ -d /media/${USER}/SYSTEM ]; then
	echo "SYSTEM mounted in /media/user"
	LIBDIR=/media/${USER}/SYSTEM/lib/
elif [ -d /run/media/${USER}/SYSTEM ]; then
	echo "SYSTEM mounted in /run/media/user"
	LIBDIR=/run/media/${USER}/SYSTEM/lib/
else
	echo "SYSTEM directory not found or mounted, quitting."
	exit
fi 

if [ -d "$BOOTDIR" ]; then
	sudo rm -f $BOOTDIR/*.dtb
	sudo rm -f $BOOTDIR/Image
else
	sudo mkdir $BOOTDIR
fi

if [ -d "$LIBDIR" ]; then
	sudo rm -rf $LIBDIR/modules
fi

sudo cp output/Image $BOOTDIR
sudo cp output/*.dtb $BOOTDIR
sudo cp -r output/lib/modules $LIBDIR
sync
