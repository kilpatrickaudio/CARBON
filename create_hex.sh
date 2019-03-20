#!/bin/bash
#
# create an unencrypted binary HEX file
#
# clean up
rm -f carbon.hex
# convert binary file to HEX
echo making HEX from binary
arm-none-eabi-objcopy --change-address 0x8020000 -Ibinary -Oihex main.bin carbon.hex
echo done!
