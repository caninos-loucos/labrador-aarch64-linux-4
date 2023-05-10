#!/bin/sh
USER=$(whoami)
sudo rm -f /media/${USER}/BOOT/*.dtb
sudo rm -rf /media/${USER}/SYSTEM/lib/modules
sudo cp output32/uImage /media/${USER}/BOOT/
sudo cp output32/*.dtb /media/${USER}/BOOT/
sudo cp -r output32/lib/modules /media/${USER}/SYSTEM/lib/
sync
