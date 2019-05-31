#!/bin/bash

IFACE="utun6"
PORT_SYNC=1111

# get individual IPs
IP_RTT=$(ping6 -I "$IFACE" -c 2 ff02::1 | awk '/DUP!/ { print $4,$7 }')
# remove prefixes and sufixes
IP_RTT="${$'IP_RTT'//time=/}"
IP_RTT="${$'IP_RTT'//: / }"

echo "$IP_RTT"


#get RTTs individually and send them to nodes
while read -r $"IP_RTT"; do
        addr=$(echo "$IP_RTT" | awk '{print $1}')
        RTT=$(ping6 -I "$IFACE" -c 1 "$addr" | awk '/time=/ { print $7 }')
#	echo "$RTT"
	RTT="${$'RTT'//time=/}"
#	echo "$RTT"
        rtt_micro=$(echo "$RTT*1000"|bc)
#	echo "$rtt_micro"
        echo "R$rtt_micro" | nc -6u "$addr" "$PORT_SYNC" &
        kill $(pidof nc)
        sleep 2
done <<< "$IP_RTT"

sleep 3

# now send NPT time to all nodes via broadcast
ntp_micro=$(($(date +%s%N)/1000))
echo "N$ntp_micro" | nc -6u "ff02::2%$IFACE" "$PORT_SYNC" &
kill $(pidof nc)

