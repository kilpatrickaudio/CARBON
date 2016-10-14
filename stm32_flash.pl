#!/usr/bin/perl
#
# Control program for using JLink to flash CARBON.
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
# NOTE: needs libnet-telnet-perl package.
use Net::Telnet;
use Cwd 'abs_path';
 
my $numArgs = $#ARGV + 1;
if($numArgs != 1) {
    die( "Usage ./stm32_flash.pl [main.bin] \n");
}

my $file = abs_path($ARGV[0]);

my $ip = "127.0.0.1";   # localhost
my $port = 4444;

my $telnet = new Net::Telnet (
    Port   => $port,
    Timeout=> 30,
    Errmode=> 'die',
    Prompt => '/>/');

$telnet->open($ip);

print $telnet->cmd('reset halt');
print $telnet->cmd('flash probe 0');
print $telnet->cmd('flash write_image erase '.$file.' 0x08000000');
print $telnet->cmd('reset');
print $telnet->cmd('exit');

print "\n";

