#!/bin/bash
#
# transfer a framebuffer to the named unix socket using socat
#
SOCKET='/run/caprica/display'
SOURCE="${1}"
if [ -e "${SOURCE}" ] ; then
  socat -u "OPEN:${SOURCE},rdonly" UNIX-SENDTO:${SOCKET}
else
  echo usage: ${0} filename
fi
