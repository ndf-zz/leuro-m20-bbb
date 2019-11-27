# leuro-m20-bbb

Hardware interface to Leurocomm M20 using 
Beaglebone Black mmap'ed GPIO with a cavalier approach.

## Overview

leuro-m20-bbb reads framebuffer updates (see related project 
[caprica](https://github.com/ndf-zz/caprica)
) sent over a Unix domain socket, then writes the framebuffer
to an attached Leurocomm M20 monochrome LED display by bit-banging
GPIO pins on a Beaglebone Black. In order to achieve acceptible
refresh times, GPIO set and clear registers are written directly
using mmap-ed pages from /dev/mem.

## Building

Edit the socket, screen configuration, and UID/GID defines,
then compile leuro-m20-bbb using make:

	$ make
	cc -pedantic -Wall -Werror -O9 leuro-m20-bbb.c -o leuro-m20-bbb

## Running

First run the provided script pinsetup.h (or an equivalent process)
to configure GPIO and PWM settings on the relevant Beaglebone Black
pins. Then, as root, run leuro-m20-bbb:

	$ ./pinsetup.sh
	$ sudo ./leuro-m20-bbb

## Hardware Setup

Make the following connections from the Beaglebone Black
P9 expansion header through a ~500 Ohm resistor[1] to the 
non-inverting inputs[2] on the "LED-ADAPTER-KARTE" or directly
to the "LED-OPTO-KARTE". If the display has a "LEDDIMMER" board,
remove it and the connecting ribbon completely.

Display Line | Function | BBB Pin | GPIO | Note
--- | --- | --- | --- | ---
E5/E5N | | n/c | | not used
E4/E4N | | n/c | | not used
E3/E3N | | n/c | | not used
E2/E2N | icard Transfer | P9.15 | 1:16 |
E1/E1N | icard Select | P9.12 | 1:28 |
E0/E0N | Row Select | P9.26 | 0:14 | if set, T clocks icard latch
S/SN | Show | P9.16 | 1:19 | connects to all panels all the time
T/TN | Transfer | P9.24 | 0:15 |
DI/DIN | Dimmer | P9.14 | pwm1A | 0: on 1: off (setup with sysfs)
HA/HAN | | n/c | | not used
HB/HBN | | n/c | | not used
D1/D1N | Row 4b | P9.11 | 0:30 | row 4 is not used in this case
D2/D2N | Row 4a | P9.13 | 0:31 |
D3/D3N | Row 3b | P9.17 | 0:5 |
D4/D4N | Row 3a | P9.18 | 0:4 | row advance through icard latch
D5/D5N | Row 2b | P9.19 | 0:13 | row 4 select on icard latch
D6/D6N | Row 2a | P9.20 | 0:12 | row 3 select on icard latch
D7/D7N | Row 1b | P9.21 | 0:3 | row 2 select on icard latch
D8/D8N | Row 1a | P9.22 | 0:2 | row 1 select on icard latch

   - [1] On the "LED-OPTO-KARTE" board, A2231 optocouplers
     decode balanced lines from a Leurocomm PCI card through a
     220 Ohm series resistor. To get the recommended
     2.5 mA "ON" current from a Beaglebone Black 3.3V GPIO pin,
     an additional ~500 Ohm series resistor is required on each input.

   - [2] Balanced inputs are labeled X/XN where X is the inverting input,
     and XN is the non-inverting input for line X. X connects to the A2231
     cathode, XN connects to the anode. For more information on the
     A2231, see the
     [manufacturer website](https://www.broadcom.com/products/optocouplers/industrial-plastic/digital-optocouplers/5mbd/hcpl-2231).

## Display Mapping

FIGURE 1

Leurocomm displays (viewed from the front-side) are arranged in rows
of LED panels labeled "LED - M20CC" (12x12 pixels in this case),
daisy chained right to left. Each LED panel has two shift registers
"a" and "b", arranged in 6 rows of 3 columns, each 4 bits wide.
Groups of 3 or 4 LED panel rows are driven from interface cards
labeled "4fach-Verteiler PCI IO -VM2003".
Interface cards are on the right side of the display, connected from
bottom to top. Each collection of rows, numbered 1-4 on the interface card,
is connected top to bottom.
Framebuffer updates from caprica describe the desired image as a buffer
of 32 bit pixel values packed little-endian[3] from left to right,
top to bottom.

   * [3] Beaglebone Black typically runs little-endian

## Hardware Reference

TODO
