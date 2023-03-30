#!/bin/sh
USER=$(whoami)

sudo cp -r output32/lib/modules /media/${USER}/SYSTEM/lib/
sudo cp output32/uImage /media/${USER}/BOOT/
sudo cp output32/kernel.dtb /media/${USER}/BOOT/
sync
