#!/bin/bash

IFACE="utun6"
PORT_SYNC=1111

# send NPT time to all nodes via broadcast
ntp_micro=$(($(date +%s%N)/1000))
echo "N$ntp_micro" | nc -6u "ff02::2%$IFACE" "$PORT_SYNC" &
kill $(pidof nc)

