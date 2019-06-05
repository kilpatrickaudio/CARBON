# CARBON
CARBON Sequencer

This is the source code for the CARBON hardware music sequencer by Kilpatrick 
Audio. Please see this link for product details:

[CARBON page at Kilpatrick Audio](http://www.kilpatrickaudio.com/?p=carbon)

## Looking for Binaries?

If you don't want to build the code yourself but just want the latest firmware to install (no programming hardware required) just visit this link: [Kilpatrick Audio Firmware Updates](http://www.kilpatrickaudio.com/?p=firmware_updates)

## Licensing / using code in your own projects

__THIS CODE IS GPL LICENSED UNLESS SPECIFIED OTHERWISE__

If you plan to use this code in your own projects you MUST understand the
requirements of each source file you use. Some vendor-supplied files have 
special license restrictions. All other source files are GPL licensed. You
must understand what this means, especially in regards to commercial projects.

In short: If you use any (non-ST supplied) code in your project, your entire
project must be licensed and released under the GPL. If you don't agree with
this please leave now and don't refer to or use any of the code here.

## Developing on CARBON Hardware

You bought a CARBON and want to hack it? Yay!

Please note that any modification you do to CARBON whether hardware or software
is entirely at your own risk. It *IS* possible to damage hardware with wrong
firmware, so please be careful and read any I/O code carefully before making
changes. The hardware is not open source but there are clear APIs for everything
related to the physical device so that should be easy enough.

If you want to try out stuff, you will most likely need to use a JTAG interface
to program and debug CARBON. The J-Link or ST-Link are both decent and support the
industry-standard 20 pin (10x2) ARM JTAG connector. You will find an unpopulated
connector on the corner of the mainboard inside CARBON labeled X10. Solder a pin
header on here and connect it to your JTAG programmer. The rest should be obvious
from reading the various scripts included with the code.

## Reporting bugs

Please open an issue for each reproducible bug. Interesting and
easily integrated pull requests will be considered for official releases.
Feature requests should not be added one at a time. Make sure your request
would make sense for most users, and keeps with the idea of the original
design. Small enhancements are considered, but major redesigns are really
beyond the scope of what we can consider.

__IMPORTANT: Do not place multiple feature / bug requests in the same issue!__

## Changelog:

For changes please see the individual release notes.
  

