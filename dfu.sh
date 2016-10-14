#!/bin/bash
#
# Script to flash CARBON over USB using DFU mode.
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

# load the binary over DFU
# device must be in DFU boot mode
dfu-util -a 0 -d 0483:df11 --dfuse-address 0x08000000 -D main.bin

