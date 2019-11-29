#!/bin/bash
#
# transfer a framebuffer to the named unix socket using socat
#
SOCKET='caprica-144x72'
SOURCE="${1}"
if [ -e "${SOURCE}" ] ; then
  socat -u "OPEN:${SOURCE},rdonly" ABSTRACT-SENDTO:${SOCKET}
else
  echo usage: ${0} filename
fi
