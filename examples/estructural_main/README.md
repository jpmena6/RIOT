Test thread connectivity
=========

#BUG

To run this example you must (only the first time):

¨make all¨

Then goto: bin/pkg/${BOARD}/openthread/src/cli/cli.cpp:266 and comment the line:

¨//otIcmp6RegisterHandler(mInstance, &mIcmpHandler);¨

Then:

make flash

OR try this fix: https://github.com/RIOT-OS/RIOT/pull/11500

And compile without CLI example

It is also necesary to make threads priority lower. goto pkg/openthread/openthread.c and lower the priority by adding value to prio: THREAD_PRIORITY_MAIN+2

Also, to use the FRDM-KW41Z you need to use:

git remote add upstream https://github.com/RIOT-OS/RIOT
git fetch upstream pull/7107/head:pr/kw41zrf
git checkout pr/kw41zrf


Then change:

 ZLL->MACSHORTADDRS0 = (ZLL->MACSHORTADDRS0 & ~ZLL_MACSHORTADDRS0_MACSHORTADDRS0_MASK) |
       ZLL_MACSHORTADDRS0_MACSHORTADDRS0( ((addr&0xff)<<8) | (addr>>8));

in kw41zrf_set_addr_short(...) in drivers/kw41zrf/kw41zrf_getset.c 

=========

Sending and receiving packages.

port 7777 is used for p2p coms between devices
port 1111 is used for coms for sync
port 8888 is used for coms to server

=========

On BorderRouter install netcat-openbsd

´sudo apt-get install netcat-openbsd´

on border router run send_rtt_ntp.sh periodically (crontab etc.)


=========

The server must be reachable by the devices at fd11::100 and devices have the address fd11:1212::/64

On border router then:
sudo ip -6 route add fd11::/64 dev eth0
wpanctl config-gateway -d "fd11:1212::"

On server: 
sudo ip -6 addr add fd11::100/64 dev eth0
sudo ip -6 route add fd11:1212::/64 via fe80::a2f6:fdff:fe8a:5f28 dev eth0


=====
