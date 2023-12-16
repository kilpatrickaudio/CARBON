#!/bin/bash
#
# Script for running the makegen program to generate a Makefile automatically.
#
# Written by: Andrew Kilpatrick
# Copyright 2015: Kilpatrick Audio
#
# This file is part of CARBON.
#
# CARBON is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# CARBON is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with CARBON.  If not, see <http://www.gnu.org/licenses/>.
#
# K66 target board - -O2 optimisation
# - generates an ARM ELF file first
# - converts ELF into bin (JLink and DFU use the .bin)
makegen -b build -c ~/bin/gcc-arm/bin/arm-none-eabi-gcc -d ~/bin/gcc-arm/bin/arm-none-eabi-gcc -a ~/bin/gcc-arm/bin/arm-none-eabi-as -i "-Isrc -Isrc/usbd_midi -Isrc/usbh_midi -IDrivers/CMSIS/Device/ST/STM32F4xx/Include -IDrivers/CMSIS/Include -IDrivers/STM32F4xx_HAL_Driver/Inc -IMiddlewares/ST/STM32_USB_Device_Library/Core/Inc -IMiddlewares/ST/STM32_USB_Host_Library/Core/Inc" -f "-O2 -g -Wall -mlittle-endian -mthumb -mthumb-interwork -mcpu=cortex-m4 -DUSE_HAL_DRIVER -DSTM32F407xx -DARM_MATH_CM4 -DUSBD_USE_FS -DUSBH_USE_HS -DUSE_USB_HS_IN_FS -DVTOR_DEV" -e makegen.exclude -r "./carbon -i 0" -l "--specs=nosys.specs -mthumb -mthumb-interwork -mcpu=cortex-m4 -Wl,-Map=main.map,--gc-sections,--no-warn-rwx-segments -Tsrc/STM32F407VE_FLASH-dev.ld" -p "~/bin/gcc-arm/bin/arm-none-eabi-objcopy -Obinary main main.bin" main > Makefile

cat Makefile | grep error
