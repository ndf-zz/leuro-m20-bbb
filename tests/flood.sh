#!/bin/bash
#
# flood the framebuffer with random junk
#
SOCKET='/run/caprica/display'
socat -b 1440 -u "OPEN:/dev/urandom,rdonly" UNIX-SENDTO:${SOCKET}

