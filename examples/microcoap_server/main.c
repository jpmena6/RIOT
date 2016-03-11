/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       CoAP example server application (using microcoap)
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @}
 */

#include <stdio.h>
#include "msg.h"
#include "net/gnrc/rpl.h"
#include "net/gnrc/netapi.h"

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

void microcoap_server_loop(void);

/* import "ifconfig" shell command, used for printing addresses */
extern int _netif_config(int argc, char **argv);

void init_net(void)
{
    uint16_t val;
    int res;
    kernel_pid_t dev = 6;

    val = 0;
    res = gnrc_netapi_set(dev, NETOPT_CHANNEL, 0, &val, sizeof(uint16_t));
    if (res < 0) {
        printf("Unable to set channel %"PRIu16", res=%d\n", val, res);
    }

    val = 0;
    res = gnrc_netapi_set(dev, NETOPT_CHANNEL_PAGE, 0, &val, sizeof(uint16_t));
    if (res < 0) {
        printf("Unable to set page %"PRIu16", res=%d\n", val, res);
    }

    val = 0x777;
    res = gnrc_netapi_set(dev, NETOPT_NID, 0, &val, sizeof(uint16_t));
    if (res < 0) {
        printf("Unable to set PAN ID 0x%"PRIx16", res=%d\n", val, res);
    }

    gnrc_rpl_init(dev);
}

int main(void)
{
    puts("RIOT microcoap example application");

    /* microcoap_server uses conn which uses gnrc which needs a msg queue */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    init_net();

    /* print network addresses */
    puts("Configured network interfaces:");
    _netif_config(0, NULL);

    /* start coap server loop */
    microcoap_server_loop();

    /* should be never reached */
    return 0;
}
