#!/bin/bash
######################################################################
#   @author: Javad Rahimi <javad321javad@gmail.com>
#   @info: A simple script to build flash image for spi flash devices
#   #@licence: GPL
######################################################################
if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]
then
	echo "Usage: ./makeflashimage.sh <flash size> <bootloader bin file> <output flash>"
	echo "Example: ./makeflashimage.sh 4096 ./cfe9338.bin rom_file.img"
fi

echo "Cleaning old image ..."
rm -rf $3

echo "Creating empty flash space..."
dd if=/dev/zero count=$1 bs=1K | tr '\000' '\377' > $3

echo "Generating flash image..."
dd if=$2 of=$3 bs=1k  conv=notrunc

echo "All done... you can burn the image by:"
echo "sudo flashrom -p linux_spi:dev=/dev/spidev1.0  -w $3"