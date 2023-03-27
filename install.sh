#!/bin/sh
USER=$(whoami)

sudo cp -r output/lib/modules /media/${USER}/linux64/lib/
sudo cp output/Image /media/${USER}/linux64/boot
sudo cp output/*.dtb /media/${USER}/linux64/boot
sync
