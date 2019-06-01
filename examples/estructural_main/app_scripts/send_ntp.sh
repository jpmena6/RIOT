#!/bin/bash

IFACE="utun6"
PORT_SYNC=1111

# send NPT time to all nodes via broadcast
ntp_micro=$(($(date +%s%N)/1000))
# mask it to 32 bit
hexmask=FFFFFFFF
mask=$((16#$hexmask))
num=$(($ntp_micro & $mask))


echo "N$num" | nc -6u "ff02::2%$IFACE" "$PORT_SYNC" &
kill $(pidof nc)

