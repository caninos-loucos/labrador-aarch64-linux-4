#!/bin/sh
USER=$(whoami)

if [ -d /media/linux64 ]; then
	echo "linux64 mounted in /media"
	BOOTDIR=/media/linux64/boot/
	LIBDIR=/media/linux64/lib/
elif [ -d /media/${USER}/linux64 ]; then
	echo "linux64 mounted in /media/user"
	BOOTDIR=/media/${USER}/linux64/boot/
	LIBDIR=/media/${USER}/linux64/lib/
elif [ -d /run/media/${USER}/linux64 ]; then
	echo "linux64 mounted in /run/media/user"
	BOOTDIR=/run/media/${USER}/linux64/boot/
	LIBDIR=/run/media/${USER}/linux64/lib/
else
	echo "linux64 directory not found or mounted, quitting."
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
