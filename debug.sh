#!/bin/bash
#
# Script to run CARBON in debug mode over JTAG.
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
arm-none-eabi-gdb -ex "target remote localhost:3333" -ex "set remote hardware-breakpoint-limit 6" -ex "set remote hardware-watchpoint-limit 4" main

