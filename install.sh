#!/bin/sh
USER=$(whoami)
sudo rm -f /media/${USER}/linux64/boot/*.dtb
sudo rm -rf /media/${USER}/linux64/lib/modules
sudo cp output/Image /media/${USER}/linux64/boot
sudo cp output/*.dtb /media/${USER}/linux64/boot
sudo cp -r output/lib/modules /media/${USER}/linux64/lib/
sync
