# leuro-m20-bbb

Hardware interface to Leurocomm M20 on Beaglebone Black

## Hardware Setup

Make the following connections from the Beaglebone Black
P9 expansion header through a ~500ohm resistor to the 
screw terminals or the optocoupler card.

Line | Function | BBB Pin | GPIO | Note
--- | --- | --- | --- | ---
D1 | Row 4b |  |  | Row 4 is not used in this case
D2 | Row 4a |  |  |
D3 | Row 3b | P9.17 | 0:5 |
D4 | Row 3a | P9.18 | 0:4 | row advance through icard latch
D5 | Row 2b | P9.19 | 0:13 | row 4 select
D6 | Row 2a | P9.20 | 0:12 | row 3 select
D7 | Row 1b | P9.21 | 0:3 | row 2 select
D8 | Row 1a | P9.22 | 0:2 | row 1 select
T | Transfer | P9.24 | 0:15 |
E0 | Row Select | P9.26 | 0:14 | if set, icard latch is written with T
E1 | icard Select | P9.12 | 1:28 |
E2 | icard Transfer | P9.15 | 1:16 |
S | Show | P9.16 | 1:19 | connects to all panels all the time
DI | Dimmer | P9.14 | pwm1A | 0->on 1->off (setup with sysfs)

