/*
 * Copyright (C) 2017 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net
 * @file
 * @brief       GNRC ContikiMAC main event loop
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @}
 */

#include <stdbool.h>
#include <errno.h>

#include "msg.h"
#include "thread.h"

#include "net/gnrc.h"
#include "net/gnrc/nettype.h"
#include "net/netdev.h"

#include "net/gnrc/netdev.h"

#include "log.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#if defined(MODULE_OD) && ENABLE_DEBUG
#include "od.h"
#endif

/* TODO Move these parameters somewhere else */
/* (Hz) frequency of channel checks */
#define CONTIKIMAC_CHANNEL_CHECK_RATE 8
/* (usec) time between each wake period */
#define CONTIKIMAC_CYCLE_TIME (1000000ul / CONTIKIMAC_CHANNEL_CHECK_RATE)
/* Maximum number of CCA operations to perform during each wake cycle */
#define CONTIKIMAC_CCA_COUNT_MAX (2u)
/* These numbers are OK for IEEE 802.15.4 O-QPSK 250 kbit/s */
/* (usec) Tc, time between each successive CCA */
#define CONTIKIMAC_CCA_SLEEP_TIME (500u)
/* (usec) Ti, time between successive retransmissions, must be less than Tc */
#define CONTIKIMAC_INTER_PACKET_INTERVAL (400u)
/* (usec) maximum time to wait for the next packet in a burst */
#define CONTIKIMAC_INTER_PACKET_DEADLINE (32000u)
/* (usec) Maximum time to remain awake after a CCA detection */
#define CONTIKIMAC_LISTEN_TIME_AFTER_PACKET_DETECTED (12500u)
/* (usec) Maximum time to keep retransmitting the same packet before giving up */
#define CONTIKIMAC_STROBE_TIME (CONTIKIMAC_CYCLE_TIME + 2 * )

/* Size of message queue */
#define CONTIKIMAC_MSG_QUEUE_SIZE 8

/* Some internal message types */
#define CONTIKIMAC_MSG_TYPE_RADIO_OFF      0xC000
#define CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK  0xC001
#define CONTIKIMAC_MSG_TYPE_RX_BEGIN       0xC002
#define CONTIKIMAC_MSG_TYPE_RX_END         0xC003
#define CONTIKIMAC_MSG_TYPE_TX_OK          0xC004
#define CONTIKIMAC_MSG_TYPE_TX_NOACK       0xC005

static void _pass_on_packet(gnrc_pktsnip_t *pkt);

/**
 * @brief   Function called by the device driver on device events
 *
 * @param[in] event     type of event
 */
static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    gnrc_netdev_t *gnrc_netdev = (gnrc_netdev_t*) dev->context;

    if (event == NETDEV_EVENT_ISR) {
        msg_t msg = { .type = NETDEV_MSG_TYPE_EVENT };

        if (msg_send(&msg, gnrc_netdev->pid) <= 0) {
            LOG_ERROR("gnrc_contikimac(%d): possibly lost interrupt", thread_getpid());
        }
    }
    else {
        DEBUG("gnrc_contikimac(%d): event triggered -> %i\n",
              thread_getpid(), event);
        switch(event) {
            case NETDEV_EVENT_RX_COMPLETE:
                {
                    gnrc_pktsnip_t *pkt = gnrc_netdev->recv(gnrc_netdev);

                    if (pkt) {
                        _pass_on_packet(pkt);
                    }
                    msg_t msg = {.type = CONTIKIMAC_MSG_TYPE_RX_END};
                    if (msg_send(&msg, gnrc_netdev->pid) <= 0) {
                        LOG_ERROR("gnrc_contikimac(%d): lost RX_END", thread_getpid());
                    }

                    break;
                }
            case NETDEV_EVENT_RX_STARTED:
                {
                    msg_t msg = {.type = CONTIKIMAC_MSG_TYPE_RX_BEGIN};
                    if (msg_send(&msg, gnrc_netdev->pid) <= 0) {
                        LOG_ERROR("gnrc_contikimac(%d): lost RX_BEGIN", thread_getpid());
                    }
                }
            case NETDEV_EVENT_TX_MEDIUM_BUSY:
#ifdef MODULE_NETSTATS_L2
                dev->stats.tx_failed++;
#endif
                break;
            case NETDEV_EVENT_TX_COMPLETE:
#ifdef MODULE_NETSTATS_L2
                dev->stats.tx_success++;
#endif
                break;
            default:
                DEBUG("gnrc_contikimac(%d): warning: unhandled event %u.\n",
                      thread_getpid(), event);
        }
    }
}

static void _pass_on_packet(gnrc_pktsnip_t *pkt)
{
    /* throw away packet if no one is interested */
    if (!gnrc_netapi_dispatch_receive(pkt->type, GNRC_NETREG_DEMUX_CTX_ALL, pkt)) {
        DEBUG("gnrc_contikimac(%d): unable to forward packet of type %i\n",
              thread_getpid(), pkt->type);
        gnrc_pktbuf_release(pkt);
        return;
    }
}

/**
 * @brief   Startup code and event loop of the gnrc_contikimac layer
 *
 * @param[in] args  expects a pointer to a netdev_t struct
 *
 * @return          never returns
 */
static void *_gnrc_contikimac_thread(void *args)
{
    DEBUG("gnrc_contikimac(%d): starting thread\n", thread_getpid());

    gnrc_netdev_t *gnrc_netdev = args;
    netdev_t *dev = gnrc_netdev->dev;

    gnrc_netdev->pid = thread_getpid();

    gnrc_netapi_opt_t *opt;
    int res;
    msg_t msg, reply, msg_queue[CONTIKIMAC_MSG_QUEUE_SIZE];

    /* setup the MAC layer's message queue */
    msg_init_queue(msg_queue, CONTIKIMAC_MSG_QUEUE_SIZE);

    msg_t msg_sleep = { .type = CONTIKIMAC_MSG_TYPE_RADIO_OFF };
    xtimer_t timer_sleep = { .target = 0, .long_target = 0};
    msg_t msg_channel_check = { .type = CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK };
    xtimer_t timer_channel_check = { .target = 0, .long_target = 0};

    /* Initialize the radio duty cycling by passing an initial event */
    msg_send(&msg_channel_check, thread_getpid());

    /* register the event callback with the device driver */
    dev->event_callback = _event_cb;
    dev->context = gnrc_netdev;

    /* register the device to the network stack*/
    gnrc_netif_add(thread_getpid());

    /* initialize low-level driver */
    dev->driver->init(dev);

    /* Enable RX- and TX-started interrupts */
    do {
        netopt_enable_t enable = NETOPT_ENABLE;
        res = dev->driver->set(dev, NETOPT_RX_START_IRQ, &enable, sizeof(enable));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): enable NETOPT_RX_START_IRQ failed: %d\n",
                      thread_getpid(), res);
        }
        res = dev->driver->set(dev, NETOPT_RX_END_IRQ, &enable, sizeof(enable));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): enable NETOPT_RX_END_IRQ failed: %d\n",
                      thread_getpid(), res);
        }
        res = dev->driver->set(dev, NETOPT_TX_START_IRQ, &enable, sizeof(enable));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): enable NETOPT_TX_START_IRQ failed: %d\n",
                      thread_getpid(), res);
        }
        res = dev->driver->set(dev, NETOPT_TX_END_IRQ, &enable, sizeof(enable));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): enable NETOPT_TX_END_IRQ failed: %d\n",
                      thread_getpid(), res);
        }
    } while(0);

    xtimer_ticks32_t last_channel_check = xtimer_now();
    bool tx_in_progress = false;
    xtimer_ticks32_t tx_timeout = xtimer_ticks(0);
    gnrc_pktsnip_t *current_tx = NULL;

    static const netopt_state_t state_sleep   = NETOPT_STATE_SLEEP;
    static const netopt_state_t state_standby = NETOPT_STATE_STANDBY;
    static const netopt_state_t state_listen  = NETOPT_STATE_IDLE;
    /* start the event loop */
    while (1) {
        DEBUG("gnrc_contikimac(%d): waiting for incoming messages\n", thread_getpid());
        msg_receive(&msg);
        /* dispatch NETDEV and NETAPI messages */
        switch (msg.type) {
            case CONTIKIMAC_MSG_TYPE_RADIO_OFF:
                DEBUG("gnrc_contikimac(%d): Going to sleep\n", thread_getpid());
                res = dev->driver->set(dev, NETOPT_STATE, &state_sleep, sizeof(state_sleep));
                break;
            case CONTIKIMAC_MSG_TYPE_RX_BEGIN:
            {
                /* postpone sleep */
                xtimer_set_msg(&timer_sleep, CONTIKIMAC_LISTEN_TIME_AFTER_PACKET_DETECTED,
                               &msg_sleep, thread_getpid());
            }
            case CONTIKIMAC_MSG_TYPE_RX_END:
            {
                /* TODO process frame pending field */
                /* postpone sleep */
                xtimer_set_msg(&timer_sleep, CONTIKIMAC_INTER_PACKET_DEADLINE,
                               &msg_sleep, thread_getpid());
            }
            case CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK:
            {
                DEBUG("gnrc_contikimac(%d): Checking channel\n", thread_getpid());
                /* Perform multiple CCA and check the results */
                /* This operation will block the thread from other events (TX
                 * etc.) until we are done */
                /* Turn on the radio */
                res = dev->driver->set(dev, NETOPT_STATE, &state_standby, sizeof(state_standby));
                if (res < 0) {
                    DEBUG("gnrc_contikimac(%d): Failed setting NETOPT_STATE_STANDBY: %d\n",
                          thread_getpid(), res);
                    break;
                }
                bool found = 0;
                for (unsigned cca = CONTIKIMAC_CCA_COUNT_MAX; cca > 0; --cca) {
                    netopt_enable_t channel_clear;
                    res = dev->driver->get(dev, NETOPT_IS_CHANNEL_CLR, &channel_clear, sizeof(channel_clear));
                    if (res < 0) {
                        DEBUG("gnrc_contikimac: Failed getting NETOPT_IS_CHANNEL_CLR: %d\n", res);
                        break;
                    }
                    if (!channel_clear) {
                        /* Detected some radio energy on the channel */
                        found = 1;
                        break;
                    }
                    xtimer_usleep(CONTIKIMAC_CCA_SLEEP_TIME);
                }
                if (found) {
                    /* Set the radio to listen for incoming packets */
                    DEBUG("gnrc_contikimac(%d): Detected, listening\n", thread_getpid());
                    res = dev->driver->set(dev, NETOPT_STATE, &state_listen, sizeof(state_listen));
                    if (res < 0) {
                        DEBUG("gnrc_contikimac(%d): Failed setting NETOPT_STATE_IDLE: %d\n",
                              thread_getpid(), res);
                        break;
                    }
                    /* go back to sleep after some time if we don't see any packets */
                    xtimer_set_msg(&timer_sleep, CONTIKIMAC_LISTEN_TIME_AFTER_PACKET_DETECTED,
                                   &msg_sleep, thread_getpid());
                }
                else {
                    /* Nothing detected, immediately return to sleep */
                    DEBUG("gnrc_contikimac(%d): Nothing, sleeping\n", thread_getpid());
                    res = dev->driver->set(dev, NETOPT_STATE, &state_sleep, sizeof(state_sleep));
                    if (res < 0) {
                        DEBUG("gnrc_contikimac(%d): Failed setting NETOPT_STATE_SLEEP: %d\n",
                              thread_getpid(), res);
                        break;
                    }
                    /* Schedule the next wake up */
                    xtimer_periodic_msg(&timer, &last_wakeup, CONTIKIMAC_CHANNEL_CHECK_INTERVAL,
                                        &sleep_msg, thread_getpid());
                }
                break;
            }
            case CONTIKIMAC_MSG_TYPE_TX_NOACK:
                break;
            case CONTIKIMAC_MSG_TYPE_TX_OK:
                /* TX done, OK to release */
                gnrc_pktbuf_release(current_tx);
                break;
            case NETDEV_MSG_TYPE_EVENT:
                DEBUG("gnrc_contikimac(%d): GNRC_NETDEV_MSG_TYPE_EVENT received\n",
                      thread_getpid());
                dev->driver->isr(dev);
                break;
            case GNRC_NETAPI_MSG_TYPE_SND:
                /* TODO set frame pending field in some circumstances */
                /* TODO enqueue packets */
                DEBUG("gnrc_contikimac(%d): GNRC_NETAPI_MSG_TYPE_SND received\n",
                      thread_getpid());
                if (tx_in_progress) {
                    LOG_ERROR("gnrc_contikimac(%d): dropped TX packet\n",
                              thread_getpid());
                    break;
                }
                current_tx = msg.content.ptr;
                /* Hold until we have confirmed this packet delivered, or too
                 * many retransmissions */
                gnrc_pktbuf_hold(current_tx, 1);
                tx_timeout = xtimer_now_usec() + ;
                tx_in_progress = true;
                gnrc_netdev->send(gnrc_netdev, current_tx);
                break;
            case GNRC_NETAPI_MSG_TYPE_SET:
                /* read incoming options */
                opt = msg.content.ptr;
                DEBUG("gnrc_contikimac(%d): GNRC_NETAPI_MSG_TYPE_SET received. opt=%s\n",
                      thread_getpid(), netopt2str(opt->opt));
                /* set option for device driver */
                res = dev->driver->set(dev, opt->opt, opt->data, opt->data_len);
                DEBUG("gnrc_contikimac(%d): response of netdev->set: %i\n",
                      thread_getpid(), res);
                /* send reply to calling thread */
                reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
                reply.content.value = (uint32_t)res;
                msg_reply(&msg, &reply);
                break;
            case GNRC_NETAPI_MSG_TYPE_GET:
                /* read incoming options */
                opt = msg.content.ptr;
                DEBUG("gnrc_contikimac(%d): GNRC_NETAPI_MSG_TYPE_GET received. opt=%s\n",
                      thread_getpid(), netopt2str(opt->opt));
                /* get option from device driver */
                res = dev->driver->get(dev, opt->opt, opt->data, opt->data_len);
                DEBUG("gnrc_contikimac(%d): response of netdev->get: %i\n",
                      thread_getpid(), res);
                /* send reply to calling thread */
                reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
                reply.content.value = (uint32_t)res;
                msg_reply(&msg, &reply);
                break;
            default:
                DEBUG("gnrc_contikimac(%d): Unknown command %" PRIu16 "\n",
                      thread_getpid(), msg.type);
                break;
        }
    }
    /* never reached */
    return NULL;
}

kernel_pid_t gnrc_contikimac_init(char *stack, int stacksize, char priority,
                        const char *name, gnrc_netdev_t *gnrc_netdev)
{
    kernel_pid_t res;

    /* check if given netdev device is defined and the driver is set */
    if (gnrc_netdev == NULL || gnrc_netdev->dev == NULL) {
        return -ENODEV;
    }

    /* create new gnrc_netdev thread */
    res = thread_create(stack, stacksize, priority, THREAD_CREATE_STACKTEST,
                        _gnrc_contikimac_thread, gnrc_netdev, name);
    if (res <= 0) {
        return -EINVAL;
    }

    return res;
}
