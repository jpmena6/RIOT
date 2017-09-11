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
#include "thread_flags.h"

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

#ifndef CONTIKIMAC_CYCLE_TIME
#ifndef CONTIKIMAC_CHANNEL_CHECK_RATE
/* (Hz) frequency of channel checks */
#define CONTIKIMAC_CHANNEL_CHECK_RATE 8
#endif
/* (usec) time between each wake period */
#define CONTIKIMAC_CYCLE_TIME (1000000ul / CONTIKIMAC_CHANNEL_CHECK_RATE)
#endif /* !defined(CONTIKIMAC_CYCLE_TIME) */

/* Maximum number of CCA operations to perform during each wake cycle */
#define CONTIKIMAC_CCA_COUNT_MAX (2u)

/* The numbers below are OK for IEEE 802.15.4 O-QPSK 250 kbit/s */

/* (usec) Tc, sleep time between each successive CCA */
#define CONTIKIMAC_CCA_SLEEP_TIME (600u)

/* (usec) Tr, time required for a single CCA check */
/* The CCA check should take exactly 8 symbols (128us), but the transceiver will
 * take some time to prepare for RX as well. */
#define CONTIKIMAC_CCA_CHECK_TIME (192u)

/* (usec) time for a complete CCA loop iteration */
#define CONTIKIMAC_CCA_CYCLE_TIME (CONTIKIMAC_CCA_SLEEP_TIME + CONTIKIMAC_CCA_CHECK_TIME)

/* (usec) Ti, time between successive retransmissions, must be less than Tc */
#define CONTIKIMAC_INTER_PACKET_INTERVAL (300u)

/* (usec) maximum time to wait for the next packet in a burst */
#define CONTIKIMAC_INTER_PACKET_DEADLINE (32000u)

/* (usec) Maximum time to remain awake after a CCA detection */
#define CONTIKIMAC_LISTEN_TIME_AFTER_PACKET_DETECTED (12500u)

/* (usec) time for a complete channel check cycle with CONTIKIMAC_CCA_COUNT_MAX number of CCAs */
#define CONTIKIMAC_TOTAL_CHECK_TIME ((CONTIKIMAC_CCA_COUNT_MAX) * (CONTIKIMAC_CCA_CYCLE_TIME))

/* (usec) Maximum time to keep retransmitting the same packet before giving up */
#define CONTIKIMAC_STROBE_TIME ((CONTIKIMAC_CYCLE_TIME) + 2 * (CONTIKIMAC_TOTAL_CHECK_TIME))

/* (usec) time to wait for Ack packet after TX has completed,
 * the standard specifies that the Ack will begin transmission exactly AIFS
 * after the packet has been received.
 * AIFS = macSifsPeriod = 12 symbols (for O-QPSK)
 * The Ack is at least 5 bytes in length and has a normal SFD (5 bytes) and PHR
 * (1 byte), which yields
 * 11 * 2 symbols = 22 symbols which gives a total time of 34 symbols.
 * The at86rf2xx has a hardware ack reception timeout of 54 symbols, other
 * software sources seem to use this number as well.
 */
#define CONTIKIMAC_ACK_WAIT_TIME (54u * 16u)

#define CONTIKIMAC_TX_TIME_PER_BYTE (2u * 16u)
/* (usec) time it takes to transmit the start of frame delimiter (SFD) and PHY
 * header (PHR) fields */
#define CONTIKIMAC_TX_PREAMBLE_TIME (12u * 16u)

/* Size of message queue */
#define CONTIKIMAC_MSG_QUEUE_SIZE 8

/* Some internal message types */
#define CONTIKIMAC_MSG_TYPE_RADIO_OFF      0xC000
#define CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK  0xC001
#define CONTIKIMAC_MSG_TYPE_RX_BEGIN       0xC002
#define CONTIKIMAC_MSG_TYPE_RX_END         0xC003

#define CONTIKIMAC_THREAD_FLAG_ISR       (1 << 0)
#define CONTIKIMAC_THREAD_FLAG_TX_NOACK  (1 << 1)
#define CONTIKIMAC_THREAD_FLAG_TX_ERROR  (1 << 2)
#define CONTIKIMAC_THREAD_FLAG_TX_OK     (1 << 3)
#define CONTIKIMAC_THREAD_FLAG_RX_BEGIN  (1 << 4)
#define CONTIKIMAC_THREAD_FLAG_RX_END    (1 << 5)

typedef struct {
    gnrc_netdev_t *gnrc_netdev;
    xtimer_ticks32_t last_channel_check;
    struct {
        xtimer_t channel_check;
        xtimer_t sleep;
    } timers;
    bool rx_in_progress;
} contikimac_context_t;

static const netopt_state_t state_standby = NETOPT_STATE_STANDBY;
static const netopt_state_t state_listen  = NETOPT_STATE_IDLE;
static const netopt_state_t state_tx      = NETOPT_STATE_TX;

static void _pass_on_packet(gnrc_pktsnip_t *pkt);

/**
 * @brief   Function called by the device driver on device events
 *
 * @param[in] event     type of event
 */
static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    gnrc_netdev_t *gnrc_netdev = dev->context;

    if (event == NETDEV_EVENT_ISR) {
        thread_flags_set((thread_t *)thread_get(gnrc_netdev->pid), CONTIKIMAC_THREAD_FLAG_ISR);
    }
    else {
        DEBUG("gnrc_contikimac(%d): event triggered -> %i\n",
              thread_getpid(), event);
        switch(event) {
            case NETDEV_EVENT_RX_COMPLETE:
                {
                    DEBUG("RXOK\n");
                    gnrc_pktsnip_t *pkt = gnrc_netdev->recv(gnrc_netdev);

                    if (pkt) {
                        _pass_on_packet(pkt);
                    }
                    msg_t msg = {.type = CONTIKIMAC_MSG_TYPE_RX_END};
                    if (msg_send(&msg, gnrc_netdev->pid) <= 0) {
                        LOG_ERROR("gnrc_contikimac(%d): lost RX_END\n", thread_getpid());
                    }
                    break;
                }
            case NETDEV_EVENT_RX_STARTED:
                {
                    msg_t msg = {.type = CONTIKIMAC_MSG_TYPE_RX_BEGIN};
                    if (msg_send(&msg, gnrc_netdev->pid) <= 0) {
                        LOG_ERROR("gnrc_contikimac(%d): lost RX_BEGIN\n", thread_getpid());
                    }
                    break;
                }
            case NETDEV_EVENT_TX_MEDIUM_BUSY:
#ifdef MODULE_NETSTATS_L2
                dev->stats.tx_failed++;
#endif
                thread_flags_set((thread_t *)thread_get(gnrc_netdev->pid), CONTIKIMAC_THREAD_FLAG_TX_ERROR);
                break;
            case NETDEV_EVENT_TX_NOACK:
#ifdef MODULE_NETSTATS_L2
                dev->stats.tx_failed++;
#endif
                thread_flags_set((thread_t *)thread_get(gnrc_netdev->pid), CONTIKIMAC_THREAD_FLAG_TX_NOACK);
                break;
            case NETDEV_EVENT_TX_COMPLETE:
#ifdef MODULE_NETSTATS_L2
                dev->stats.tx_success++;
#endif
                thread_flags_set((thread_t *)thread_get(gnrc_netdev->pid), CONTIKIMAC_THREAD_FLAG_TX_OK);
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
 * @brief  Transmit a packet until timeout or an Ack has been received
 *
 * @pre Packet data has been preloaded
 *
 * @param[in]  dev  network device
 */
static void gnrc_contikimac_send(netdev_t *dev, gnrc_pktsnip_t *pkt)
{
    gnrc_netif_hdr_t *netif_hdr = pkt->data;
    bool broadcast = ((netif_hdr->flags &
        (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST)));

    xtimer_ticks32_t tx_timeout = xtimer_ticks_from_usec(xtimer_now_usec() + CONTIKIMAC_STROBE_TIME);
    bool do_transmit = true;
    uint32_t time_before = 0;
    time_before = xtimer_now_usec();
    xtimer_ticks32_t last_irq;
    do {
        thread_flags_t txflags;
        if (do_transmit) {
//             time_before = xtimer_now_usec();
            uint32_t time_after = xtimer_now_usec();
            LOG_ERROR("S: %lu\n", time_after - time_before);
            do_transmit = false;
            thread_flags_clear(CONTIKIMAC_THREAD_FLAG_TX_NOACK |
                               CONTIKIMAC_THREAD_FLAG_TX_ERROR |
                               CONTIKIMAC_THREAD_FLAG_TX_OK);
            time_before = xtimer_now_usec();
            dev->driver->set(dev, NETOPT_STATE, &state_tx, sizeof(state_tx));
         txflags = thread_flags_wait_any(
            CONTIKIMAC_THREAD_FLAG_TX_NOACK | CONTIKIMAC_THREAD_FLAG_TX_OK |
            CONTIKIMAC_THREAD_FLAG_ISR);
        }
        else{
         txflags = thread_flags_wait_any(
            CONTIKIMAC_THREAD_FLAG_TX_NOACK | CONTIKIMAC_THREAD_FLAG_TX_OK |
            CONTIKIMAC_THREAD_FLAG_ISR);
        }
        if (txflags & CONTIKIMAC_THREAD_FLAG_ISR) {
            /* To get the wait timing right we will save the timestamp here.
             * The time of the last IRQ before the TX_OK or TX_NOACK flag was
             * set is used as an approximation of when the TX operation finished */
            last_irq = xtimer_now();
            /* Let the driver handle the IRQ */
            dev->driver->isr(dev);
        }
        /* note: intentionally not an else if, the ISR flag may become set again
         * by the driver after the TX_xxx flag has been set. */
        if (txflags & CONTIKIMAC_THREAD_FLAG_TX_OK) {
            /* For unicast, stop after receiving the first Ack */
            if (!broadcast) {
                uint32_t time_after = xtimer_now_usec();
                LOG_ERROR("O: %lu\n", time_after - time_before);
                break;
            }
            /* For broadcast and multicast, always transmit for the full
             * duration of STROBE_TIME. */
            do_transmit = true;
            /* Wait for a short while before retransmitting */
            xtimer_periodic_wakeup(&last_irq, CONTIKIMAC_INTER_PACKET_INTERVAL);
        }
        /* note: else if is intentional, only one of TX_OK, TX_NOACK, TX_ERROR
         * should be handled, they all result in retransmissions, only the
         * timing differs */
        else if (txflags & CONTIKIMAC_THREAD_FLAG_TX_NOACK) {
            /* retransmit */
            do_transmit = true;
            /* the Ack timeout has already passed since the actual TX
             * completed, we need to subtract that time from the sleep interval */
//             xtimer_periodic_wakeup(&last_irq, CONTIKIMAC_INTER_PACKET_INTERVAL - CONTIKIMAC_ACK_WAIT_TIME);
        }
        else if (txflags & CONTIKIMAC_THREAD_FLAG_TX_ERROR) {
            /* Medium was busy or TX error */
            do_transmit = true;
            /* Skip wait on TX errors */
        }
        /* Keep retransmitting until STROBE_TIME has passed, or until we
         * receive an Ack. */
    } while(xtimer_less(xtimer_now(), tx_timeout));
}

void gnrc_contikimac_radio_sleep(netdev_t *dev)
{
    static const netopt_state_t state_sleep = NETOPT_STATE_SLEEP;
    DEBUG("gnrc_contikimac(%d): Going to sleep\n", thread_getpid());
    int res = dev->driver->set(dev, NETOPT_STATE, &state_sleep, sizeof(state_sleep));
    if (res < 0) {
        DEBUG("gnrc_contikimac(%d): Failed setting NETOPT_STATE_SLEEP: %d\n",
              thread_getpid(), res);
    }
    LED0_OFF;
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
    contikimac_context_t ctx = {
        .gnrc_netdev = gnrc_netdev,
        .timers = {
            .sleep = { .target = 0, .long_target = 0},
            .channel_check = { .target = 0, .long_target = 0},
        },
    };
    netdev_t *dev = gnrc_netdev->dev;

    gnrc_netdev->pid = thread_getpid();

    msg_t msg, reply, msg_queue[CONTIKIMAC_MSG_QUEUE_SIZE];

    msg_t msg_sleep = { .type = CONTIKIMAC_MSG_TYPE_RADIO_OFF };
    msg_t msg_channel_check = { .type = CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK };


    /* setup the MAC layer's message queue */
    msg_init_queue(msg_queue, CONTIKIMAC_MSG_QUEUE_SIZE);

    /* register the event callback with the device driver */
    dev->event_callback = _event_cb;
    dev->context = gnrc_netdev;

    /* Initialize the radio duty cycling by passing an initial event */
    msg_send(&msg_channel_check, thread_getpid());

    /* register the device to the network stack*/
    gnrc_netif_add(thread_getpid());

    /* initialize low-level driver */
    dev->driver->init(dev);

    /* Enable RX- and TX-started interrupts */
    static const netopt_enable_t enable = NETOPT_ENABLE;
    static const netopt_enable_t disable = NETOPT_DISABLE;
    static const uint8_t zero = 0;

    {
        int res;
        res = dev->driver->set(dev, NETOPT_CSMA, &disable, sizeof(disable));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): disable NETOPT_CSMA failed: %d\n",
                      thread_getpid(), res);
        }
        res = dev->driver->set(dev, NETOPT_RETRANS, &zero, sizeof(zero));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): disable NETOPT_RETRANS failed: %d\n",
                      thread_getpid(), res);
        }
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
        res = dev->driver->set(dev, NETOPT_TX_END_IRQ, &enable, sizeof(enable));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): enable NETOPT_TX_END_IRQ failed: %d\n",
                      thread_getpid(), res);
        }
        res = dev->driver->set(dev, NETOPT_PRELOADING, &enable, sizeof(enable));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): enable NETOPT_PRELOADING failed: %d\n",
                      thread_getpid(), res);
            LOG_ERROR("gnrc_contikimac requires NETOPT_PRELOADING, this node will "
            "likely not be able to communicate with other nodes!\n");
        }
    }

    ctx.last_channel_check = xtimer_now();

    /* start the event loop */
    while (1) {
        DEBUG("gnrc_contikimac(%d): waiting for events\n", thread_getpid());
        thread_flags_t flags = thread_flags_wait_any(
            THREAD_FLAG_MSG_WAITING | CONTIKIMAC_THREAD_FLAG_ISR);
        if (flags & CONTIKIMAC_THREAD_FLAG_ISR) {
            DEBUG("gnrc_contikimac(%d): ISR event\n", thread_getpid());
            dev->driver->isr(dev);
        }
        while (msg_try_receive(&msg) > 0) {
            /* dispatch NETDEV and NETAPI messages */
            switch (msg.type) {
                case CONTIKIMAC_MSG_TYPE_RADIO_OFF:
                    gnrc_contikimac_radio_sleep(dev);
                    break;
                case CONTIKIMAC_MSG_TYPE_RX_BEGIN:
                    /* postpone sleep */
                    xtimer_set_msg(&ctx.timers.sleep,
                                   CONTIKIMAC_LISTEN_TIME_AFTER_PACKET_DETECTED,
                                   &msg_sleep, thread_getpid());
                    break;
                case CONTIKIMAC_MSG_TYPE_RX_END:
                    /* TODO process frame pending field */
                    /* postpone sleep */
                    xtimer_set_msg(&ctx.timers.sleep, CONTIKIMAC_INTER_PACKET_DEADLINE,
                                &msg_sleep, thread_getpid());
                    break;
                case CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK:
                {
                    DEBUG("gnrc_contikimac(%d): Checking channel\n", thread_getpid());
                    /* Perform multiple CCA and check the results */
                    /* This operation will block the thread from other events (TX
                    * etc.) until we are done */
                    /* Turn on the radio */
                    int res = dev->driver->set(dev, NETOPT_STATE, &state_standby, sizeof(state_standby));
                    if (res < 0) {
                        LOG_ERROR("gnrc_contikimac(%d): Failed setting NETOPT_STATE_STANDBY: %d\n",
                            thread_getpid(), res);
                        break;
                    }
                    bool found = 0;
                    LED0_ON;
                    for (unsigned cca = CONTIKIMAC_CCA_COUNT_MAX; cca > 0; --cca) {
                        netopt_enable_t channel_clear;
                        res = dev->driver->get(dev, NETOPT_IS_CHANNEL_CLR, &channel_clear, sizeof(channel_clear));
                        if (res < 0) {
                            LOG_ERROR("gnrc_contikimac(%d): Failed getting NETOPT_IS_CHANNEL_CLR: %d\n",
                                thread_getpid(), res);
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
                            LOG_ERROR("gnrc_contikimac(%d): Failed setting NETOPT_STATE_IDLE: %d\n",
                                thread_getpid(), res);
                            break;
                        }
                        /* go back to sleep after some time if we don't see any packets */
                        xtimer_set_msg(&ctx.timers.sleep, CONTIKIMAC_LISTEN_TIME_AFTER_PACKET_DETECTED,
                                    &msg_sleep, thread_getpid());
                    }
                    else {
                        /* Nothing detected, immediately return to sleep */
                        DEBUG("gnrc_contikimac(%d): Nothing seen\n", thread_getpid());
                        gnrc_contikimac_radio_sleep(dev);
                    }
                    /* Schedule the next wake up */
                    xtimer_periodic_msg(&ctx.timers.channel_check, &ctx.last_channel_check,
                                        CONTIKIMAC_CYCLE_TIME,
                                        &msg_channel_check, thread_getpid());
                    break;
                }
                case NETDEV_MSG_TYPE_EVENT:
                    DEBUG("gnrc_contikimac(%d): GNRC_NETDEV_MSG_TYPE_EVENT received\n",
                        thread_getpid());
                    dev->driver->isr(dev);
                    break;
                case GNRC_NETAPI_MSG_TYPE_SND:
                {
                    /* TODO set frame pending field in some circumstances */
                    /* TODO enqueue packets */
                    DEBUG("gnrc_contikimac(%d): GNRC_NETAPI_MSG_TYPE_SND received\n",
                        thread_getpid());
                    /* Hold until we are done */
                    /* TODO cache the important info */
                    gnrc_pktsnip_t *pkt = msg.content.ptr;
                    gnrc_pktbuf_hold(pkt, 1);

                    netopt_state_t old_state;
                    int res = dev->driver->get(dev, NETOPT_STATE, &old_state, sizeof(old_state));
                    if (res < 0) {
                        DEBUG("gnrc_contikimac(%d): Failed getting NETOPT_STATE: %d\n",
                              thread_getpid(), res);
                    }
                    /*
                     * Go to standby before transmitting to avoid having incoming
                     * packets corrupt the frame buffer on single buffered
                     * transceivers (e.g. at86rf2xx). Also works around an issue
                     * on at86rf2xx where the frame buffer is lost after the
                     * first transmission because the driver puts the transceiver
                     * in sleep mode.
                     */
                    res = dev->driver->set(dev, NETOPT_STATE, &state_standby, sizeof(state_standby));
                    if (res < 0) {
                        DEBUG("gnrc_contikimac(%d): Failed setting NETOPT_STATE_STANDBY: %d\n",
                              thread_getpid(), res);
                    }
                    gnrc_netdev->send(gnrc_netdev, pkt);
                    gnrc_contikimac_send(dev, pkt);
                    /* Restore old state */
                    res = dev->driver->set(dev, NETOPT_STATE, &old_state, sizeof(old_state));
                    if (res < 0) {
                        DEBUG("gnrc_contikimac(%d): Failed setting NETOPT_STATE %u: %d\n",
                              thread_getpid(), (unsigned)old_state, res);
                    }
                    gnrc_pktbuf_release(pkt);
                    break;
                }
                case GNRC_NETAPI_MSG_TYPE_SET:
                {
                    /* read incoming options */
                    gnrc_netapi_opt_t *opt = msg.content.ptr;
                    DEBUG("gnrc_contikimac(%d): GNRC_NETAPI_MSG_TYPE_SET received. opt=%s\n",
                        thread_getpid(), netopt2str(opt->opt));
                    /* set option for device driver */
                    int res = dev->driver->set(dev, opt->opt, opt->data, opt->data_len);
                    DEBUG("gnrc_contikimac(%d): response of netdev->set: %i\n",
                        thread_getpid(), res);
                    /* send reply to calling thread */
                    reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
                    reply.content.value = (uint32_t)res;
                    msg_reply(&msg, &reply);
                    break;
                }
                case GNRC_NETAPI_MSG_TYPE_GET:
                {
                    /* read incoming options */
                    gnrc_netapi_opt_t *opt = msg.content.ptr;
                    DEBUG("gnrc_contikimac(%d): GNRC_NETAPI_MSG_TYPE_GET received. opt=%s\n",
                        thread_getpid(), netopt2str(opt->opt));
                    /* get option from device driver */
                    int res = dev->driver->get(dev, opt->opt, opt->data, opt->data_len);
                    DEBUG("gnrc_contikimac(%d): response of netdev->get: %i\n",
                        thread_getpid(), res);
                    /* send reply to calling thread */
                    reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
                    reply.content.value = (uint32_t)res;
                    msg_reply(&msg, &reply);
                    break;
                }
                default:
                    DEBUG("gnrc_contikimac(%d): Unknown command %" PRIu16 "\n",
                        thread_getpid(), msg.type);
                    break;
            }
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
