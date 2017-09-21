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

#include "net/gnrc/contikimac/contikimac.h"
#include "net/gnrc/netdev.h"

#include "log.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"
/* Set to 1 to enable debug prints of the time spent in radio ON modes */
#define ENABLE_TIMING_INFO (0)

/* Set to 1 to use LED0_ON/LED0_OFF for a visual feedback of the radio power state */
#ifndef CONTIKIMAC_DEBUG_LEDS
#define CONTIKIMAC_DEBUG_LEDS 0
#endif

#if ENABLE_TIMING_INFO
#define TIMING_PRINTF LOG_ERROR
#else
#define TIMING_PRINTF(...)
#endif

#if CONTIKIMAC_DEBUG_LEDS
#define CONTIKIMAC_LED_ON LED0_ON
#define CONTIKIMAC_LED_OFF LED0_OFF
#else
#define CONTIKIMAC_LED_ON
#define CONTIKIMAC_LED_OFF
#endif

#if defined(MODULE_OD) && ENABLE_DEBUG
#include "od.h"
#endif

/* Size of message queue */
#define CONTIKIMAC_MSG_QUEUE_SIZE 8

/* Some internal message types */
#define CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK  0xC001
#define CONTIKIMAC_MSG_TYPE_RX_BEGIN       0xC002
#define CONTIKIMAC_MSG_TYPE_RX_END         0xC003

/* Some thread flags which are used to handle events */
/* Check these definitions for collisions in case the network device driver uses
 * thread flags as well */
#define CONTIKIMAC_THREAD_FLAG_ISR       (1 << 0)
#define CONTIKIMAC_THREAD_FLAG_TICK      (1 << 1)
/* The two TX_STATUS flags are used together as a bitfield */
#define CONTIKIMAC_THREAD_FLAG_TX_STATUS (3 << 2)
#define CONTIKIMAC_THREAD_FLAG_TX_OK     (1 << 2)
#define CONTIKIMAC_THREAD_FLAG_TX_NOACK  (2 << 2)
#define CONTIKIMAC_THREAD_FLAG_TX_ERROR  (3 << 2)

/**
 * @brief Context information about the state of the MAC layer
 *
 * Context will be stack allocated inside the thread to avoid polluting the
 * public netdev_t definitions with ContikiMAC specific variables.
 */
typedef struct {
    gnrc_netdev_t *gnrc_netdev;
    const contikimac_params_t *params;
    thread_t *thread;
    xtimer_ticks32_t last_channel_check;
    xtimer_ticks32_t last_tick;
    struct {
        xtimer_t channel_check;
        xtimer_t tick;
        xtimer_t timeout;
    } timers;
    bool seen_silence;
    bool rx_in_progress;
    bool timeout_flag;
    bool no_sleep;
} contikimac_context_t;

/* Internal constants used for the netdev set NETOPT_STATE calls, as it requires
 * a pointer to the value as argument */
static const netopt_state_t state_standby = NETOPT_STATE_STANDBY;
static const netopt_state_t state_listen  = NETOPT_STATE_IDLE;
static const netopt_state_t state_tx      = NETOPT_STATE_TX;

/**
 * @brief Internal helper used for passing the received packets to the next layer
 */
static void _pass_on_packet(gnrc_pktsnip_t *pkt);

/**
 * @brief Put the radio to sleep immediately
 */
static void gnrc_contikimac_radio_sleep(netdev_t *dev);

/**
 * @brief  Transmit a packet until timeout or an Ack has been received
 *
 * @pre Packet data has been preloaded
 *
 * @param[in]  dev  network device
 */
static void gnrc_contikimac_send(contikimac_context_t *ctx, gnrc_pktsnip_t *pkt);

/**
 * @brief Periodic handler during wake times to determine when to go back to sleep
 */
static void gnrc_contikimac_tick(contikimac_context_t *ctx);

/**
 * @brief Set all network interface options that ContikiMAC uses
 */
static void setup_netdev(netdev_t *dev);

/**
 * @brief   Function called by the device driver on device events
 *
 * @param[in] event     type of event
 */
static void cb_event(netdev_t *dev, netdev_event_t event);

/**
 * @brief xtimer callback for setting a thread flag
 */
static void cb_set_tick_flag(void *arg);

/**
 * @brief xtimer callback for timeouts during fast sleep
 */
static void cb_timeout(void *arg);

static uint32_t time_begin = 0;

static void cb_event(netdev_t *dev, netdev_event_t event)
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

static void gnrc_contikimac_radio_sleep(netdev_t *dev)
{
    static const netopt_state_t state_sleep = NETOPT_STATE_SLEEP;
    DEBUG("gnrc_contikimac(%d): Going to sleep\n", thread_getpid());
    int res = dev->driver->set(dev, NETOPT_STATE, &state_sleep, sizeof(state_sleep));
    if (res < 0) {
        DEBUG("gnrc_contikimac(%d): Failed setting NETOPT_STATE_SLEEP: %d\n",
              thread_getpid(), res);
    }
    CONTIKIMAC_LED_OFF;
    TIMING_PRINTF("r: %lu\n", (unsigned long)xtimer_now_usec() - time_begin);
}

static void gnrc_contikimac_send(contikimac_context_t *ctx, gnrc_pktsnip_t *pkt)
{
    netdev_t *dev = ctx->gnrc_netdev->dev;
    gnrc_netif_hdr_t *netif_hdr = pkt->data;
    bool broadcast = ((netif_hdr->flags &
        (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST)));

    bool do_transmit = true;
    uint32_t time_before = 0;
    (void) time_before;
    if (ENABLE_TIMING_INFO) {
        time_before = xtimer_now_usec();
    }
    xtimer_ticks32_t last_irq = {0};
    /* TX aborts listening mode */
    xtimer_remove(&ctx->timers.tick);
    if (ctx->rx_in_progress) {
        /* TODO let the in progress RX frame finish before starting TX */
    }
    xtimer_remove(&ctx->timers.timeout);
    thread_flags_clear(CONTIKIMAC_THREAD_FLAG_TICK);
    ctx->timeout_flag = false;
    /* Set timeout for TX operation */
    xtimer_set(&ctx->timers.timeout, ctx->params->channel_check_period + 2 * ctx->params->cca_cycle_period);
    while(!ctx->timeout_flag) {
        thread_flags_t txflags;
        if (do_transmit) {
            /* For extra verbose TX timing info: */
            /*
            TIMING_PRINTF("S: %lu\n", (unsigned long)xtimer_now_usec() - time_before);
            */
            do_transmit = false;
            thread_flags_clear(CONTIKIMAC_THREAD_FLAG_TX_STATUS);
            if (ENABLE_TIMING_INFO) {
                time_before = xtimer_now_usec();
            }
            dev->driver->set(dev, NETOPT_STATE, &state_tx, sizeof(state_tx));
        }
        txflags = thread_flags_wait_any(
            CONTIKIMAC_THREAD_FLAG_TX_STATUS |
            CONTIKIMAC_THREAD_FLAG_ISR |
            CONTIKIMAC_THREAD_FLAG_TICK);
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
        switch (txflags & CONTIKIMAC_THREAD_FLAG_TX_STATUS) {
            case CONTIKIMAC_THREAD_FLAG_TX_OK:
                /* For unicast, stop after receiving the first Ack */
                if (!broadcast) {
                    TIMING_PRINTF("O: %lu\n", (unsigned long)xtimer_now_usec() - time_before);
                    break;
                }
                /* For broadcast and multicast, always transmit for the full strobe
                 * duration, but wait for a short while before retransmitting */
                xtimer_periodic_wakeup(&last_irq, ctx->params->inter_packet_interval);
                /* fall through */
            case CONTIKIMAC_THREAD_FLAG_TX_NOACK:
            case CONTIKIMAC_THREAD_FLAG_TX_ERROR:
                /* Skip wait on TX errors */
                /* Consider the inter_packet_interval already passed without calling
                 * xtimer to verify. Modify this part if inter_packet_interval is much
                 * longer than the Ack timeout */
                /* retransmit */
                do_transmit = true;
                break;
            default:
                /* Still waiting to hear back from the TX operation */
                break;
        }
        /* Keep retransmitting until the strobe time has passed, or until we
         * receive an Ack. */
    }
}

static void gnrc_contikimac_tick(contikimac_context_t *ctx)
{
    netdev_t *dev = ctx->gnrc_netdev->dev;
    /* Periodically perform CCA checks to evaluate channel usage */
    if (ctx->timeout_flag) {
        xtimer_remove(&ctx->timers.tick);
        xtimer_remove(&ctx->timers.timeout);
        thread_flags_clear(CONTIKIMAC_THREAD_FLAG_TICK);
        //~ LOG_ERROR("t: %lu\tc: %lu\n", (unsigned long)xtimer_now_usec() - time_begin, tick_count);
        if (ctx->rx_in_progress) {
            LOG_ERROR("gnrc_contikimac(%d): RX timeout\n",
                      thread_getpid());
        }
        else if (ctx->seen_silence) {
            LOG_ERROR("gnrc_contikimac(%d): Fast sleep (long silence)\n",
                      thread_getpid());
        }
        else {
            LOG_ERROR("gnrc_contikimac(%d): Fast sleep (noise)\n",
                      thread_getpid());
        }
        gnrc_contikimac_radio_sleep(dev);
    }
    else {
        /* We have detected some energy on the channel, we will keep checking
         * the channel periodically until it is idle, then switch to listen state */
        /* Performing a CCA check while a packet is being received may cause the
         * driver to abort the reception, we will only do CCAs while waiting for
         * the first silence */
        netopt_enable_t channel_clear;
        int res = dev->driver->get(dev, NETOPT_IS_CHANNEL_CLR, &channel_clear, sizeof(channel_clear));
        if (res < 0) {
            LOG_ERROR("gnrc_contikimac(%d): Failed getting NETOPT_IS_CHANNEL_CLR: %d\n",
                      thread_getpid(), res);
            return;
        }
        if (channel_clear) {
            /* Silence detected */
            /* We have detected an idle channel, expect incoming traffic very soon */
            int res = dev->driver->set(dev, NETOPT_STATE, &state_listen, sizeof(state_listen));
            if (res < 0) {
                LOG_ERROR("gnrc_contikimac(%d): Failed setting NETOPT_STATE_IDLE: %d\n",
                          thread_getpid(), res);
                return;
            }
            /* Set timeout in case we only detected noise */
            xtimer_set(&ctx->timers.timeout, ctx->params->listen_timeout);
        }
        else {
            /* Do next CCA */
            xtimer_periodic(&ctx->timers.tick, &ctx->last_tick, ctx->params->after_ed_scan_interval);
        }
    }
}

static void setup_netdev(netdev_t *dev)
{
    /* Enable RX- and TX-started interrupts */
    static const netopt_enable_t enable = NETOPT_ENABLE;
    static const netopt_enable_t disable = NETOPT_DISABLE;
    static const uint8_t zero = 0;

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

static void cb_set_tick_flag(void *arg)
{
    thread_t *thread = arg;
    thread_flags_set(thread, CONTIKIMAC_THREAD_FLAG_TICK);
}

static void cb_timeout(void *arg)
{
    contikimac_context_t *ctx = arg;
    ctx->timeout_flag = true;
    thread_flags_set(ctx->thread, CONTIKIMAC_THREAD_FLAG_TICK);
    DEBUG("TO\n");
}

/**
 * @brief   Startup code and event loop of the gnrc_contikimac layer
 *
 * @param[in] args  expects a pointer to a netdev_t struct
 *
 * @return          never returns
 */
static void *_gnrc_contikimac_thread(void *arg)
{
    DEBUG("gnrc_contikimac(%d): starting thread\n", thread_getpid());

    gnrc_netdev_t *gnrc_netdev = arg;
    contikimac_context_t ctx = {
        .params = &contikimac_params_OQPSK250,
        .gnrc_netdev = gnrc_netdev,
        .timers = {
            .tick = {
                .target = 0,
                .long_target = 0,
                .callback = cb_set_tick_flag,
            },
            .channel_check = { .target = 0, .long_target = 0},
            .timeout = {
                .target = 0,
                .long_target = 0,
                .callback = cb_timeout,
            },
        },
        .no_sleep = false,
    };
    thread_yield();
    ctx.thread = (thread_t *)thread_get(thread_getpid());
    ctx.timers.timeout.arg = &ctx;
    ctx.timers.tick.arg = ctx.thread;
    netdev_t *dev = gnrc_netdev->dev;

    gnrc_netdev->pid = thread_getpid();

    msg_t msg, msg_queue[CONTIKIMAC_MSG_QUEUE_SIZE];

    msg_t msg_channel_check = { .type = CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK };

    /* setup the MAC layer's message queue */
    msg_init_queue(msg_queue, CONTIKIMAC_MSG_QUEUE_SIZE);

    /* register the event callback with the device driver */
    dev->event_callback = cb_event;
    dev->context = gnrc_netdev;

    /* Initialize the radio duty cycling by passing an initial event */
    msg_send(&msg_channel_check, thread_getpid());

    /* register the device to the network stack*/
    gnrc_netif_add(thread_getpid());

    /* initialize low-level driver */
    dev->driver->init(dev);

    setup_netdev(dev);

    ctx.last_channel_check = xtimer_now();

    /* start the event loop */
    while (1) {
        DEBUG("gnrc_contikimac(%d): waiting for events\n", thread_getpid());
        thread_flags_t flags = thread_flags_wait_any(
            THREAD_FLAG_MSG_WAITING |
            CONTIKIMAC_THREAD_FLAG_ISR |
            CONTIKIMAC_THREAD_FLAG_TICK);
        if (flags & CONTIKIMAC_THREAD_FLAG_ISR) {
            DEBUG("gnrc_contikimac(%d): ISR event\n", thread_getpid());
            dev->driver->isr(dev);
        }
        while (msg_try_receive(&msg) > 0) {
            /* dispatch NETDEV and NETAPI messages */
            switch (msg.type) {
                case CONTIKIMAC_MSG_TYPE_RX_BEGIN:
                    if (ctx.no_sleep) {
                        break;
                    }
                    ctx.rx_in_progress = true;
                    xtimer_remove(&ctx.timers.tick);
                    xtimer_remove(&ctx.timers.timeout);
                    thread_flags_clear(CONTIKIMAC_THREAD_FLAG_TICK);
                    ctx.timeout_flag = false;
                    /* Set a timeout for the currently in progress RX frame */
                    xtimer_set(&ctx.timers.timeout, ctx.params->rx_timeout);
                    DEBUG("RB\n");
                    break;
                case CONTIKIMAC_MSG_TYPE_RX_END:
                    if (ctx.no_sleep) {
                        break;
                    }
                    /* TODO process frame pending field */
                    ctx.rx_in_progress = false;
                    /* We received a packet, stop checking the channel and go back to sleep */
                    xtimer_remove(&ctx.timers.tick);
                    xtimer_remove(&ctx.timers.timeout);
                    thread_flags_clear(CONTIKIMAC_THREAD_FLAG_TICK);
                    DEBUG("RE\n");
                    gnrc_contikimac_radio_sleep(dev);
                    TIMING_PRINTF("u: %lu\n", (unsigned long)xtimer_now_usec() - time_begin);
                    break;
                case CONTIKIMAC_MSG_TYPE_CHANNEL_CHECK:
                {
                    if (ctx.no_sleep) {
                        break;
                    }

                    if (ENABLE_TIMING_INFO) {
                        time_begin = xtimer_now_usec();
                    }
                    DEBUG("gnrc_contikimac(%d): Checking channel\n", thread_getpid());
                    /* Perform multiple CCA and check the results */
                    /* This resets the tick sequence */
                    /* Take the radio out of sleep mode */
                    int res = dev->driver->set(dev, NETOPT_STATE, &state_standby, sizeof(state_standby));
                    if (res < 0) {
                        LOG_ERROR("gnrc_contikimac(%d): Failed setting NETOPT_STATE_STANDBY: %d\n",
                            thread_getpid(), res);
                        break;
                    }
                    CONTIKIMAC_LED_ON;
                    bool found = false;
                    xtimer_ticks32_t last_wakeup = xtimer_now();
                    for (unsigned cca = ctx.params->cca_count_max; cca > 0; --cca) {
                        netopt_enable_t channel_clear;
                        res = dev->driver->get(dev, NETOPT_IS_CHANNEL_CLR, &channel_clear, sizeof(channel_clear));
                        if (res < 0) {
                            LOG_ERROR("gnrc_contikimac(%d): Failed getting NETOPT_IS_CHANNEL_CLR: %d\n",
                                thread_getpid(), res);
                            break;
                        }
                        if (!channel_clear) {
                            /* Detected some radio energy on the channel */
                            found = true;
                            break;
                        }
                        xtimer_periodic_wakeup(&last_wakeup, ctx.params->cca_cycle_period);
                    }
                    if (found) {
                        /* Set the radio to listen for incoming packets */
                        DEBUG("gnrc_contikimac(%d): Detected, looking for silence\n", thread_getpid());
                        ctx.last_tick = xtimer_now();
                        ctx.rx_in_progress = false;
                        ctx.seen_silence = false;
                        ctx.timeout_flag = false;
                        thread_flags_set(ctx.thread, CONTIKIMAC_THREAD_FLAG_TICK);
                        /* Set timeout in case we only detected noise */
                        xtimer_set(&ctx.timers.timeout, ctx.params->after_ed_scan_timeout);
                    }
                    else {
                        /* Nothing detected, immediately return to sleep */
                        DEBUG("gnrc_contikimac(%d): Nothing seen\n", thread_getpid());
                        gnrc_contikimac_radio_sleep(dev);
                    }
                    /* Schedule the next wake up */
                    xtimer_periodic_msg(&ctx.timers.channel_check, &ctx.last_channel_check,
                                        ctx.params->channel_check_period,
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
                    LOG_ERROR("TX\n");
                    gnrc_netdev->send(gnrc_netdev, pkt);
                    gnrc_contikimac_send(&ctx, pkt);
                    /* Restore old state */
                    if (old_state == NETOPT_STATE_RX) {
                        /* go back to idle if old state was RX in progress */
                        old_state = NETOPT_STATE_IDLE;
                    }
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
                    int res;
                    switch (opt->opt) {
                        case NETOPT_MAC_NO_SLEEP:
                            assert(opt->data_len >= sizeof(netopt_enable_t));
                            ctx.no_sleep = *((const netopt_enable_t *)opt->data);
                            /* Reset the radio duty cycling state */
                            xtimer_remove(&ctx.timers.tick);
                            xtimer_remove(&ctx.timers.channel_check);
                            ctx.rx_in_progress = false;
                            ctx.seen_silence = false;
                            ctx.timeout_flag = false;
                            thread_flags_clear(CONTIKIMAC_THREAD_FLAG_TICK);
                            if (ctx.no_sleep) {
                                /* switch the radio to listen state */
                                res = dev->driver->set(dev, NETOPT_STATE, &state_listen, sizeof(state_listen));
                                if (res < 0) {
                                    DEBUG("gnrc_contikimac(%d): Failed setting NETOPT_STATE_IDLE: %d\n",
                                          thread_getpid(), res);
                                }
                            }
                            else {
                                /* Start the radio duty cycling by passing an initial event */
                                msg_send(&msg_channel_check, thread_getpid());
                                ctx.last_channel_check = xtimer_now();
                            }
                            res = sizeof(netopt_enable_t);
                            break;
                        default:
                            /* set option for device driver */
                            res = dev->driver->set(dev, opt->opt, opt->data, opt->data_len);
                            break;
                    }
                    DEBUG("gnrc_contikimac(%d): response of netdev->set: %i\n",
                        thread_getpid(), res);
                    /* send reply to calling thread */
                    msg_t reply = {
                        .type = GNRC_NETAPI_MSG_TYPE_ACK,
                        .content.value = (uint32_t)res,
                    };
                    msg_reply(&msg, &reply);
                    break;
                }
                case GNRC_NETAPI_MSG_TYPE_GET:
                {
                    /* read incoming options */
                    gnrc_netapi_opt_t *opt = msg.content.ptr;
                    DEBUG("gnrc_contikimac(%d): GNRC_NETAPI_MSG_TYPE_GET received. opt=%s\n",
                        thread_getpid(), netopt2str(opt->opt));
                    int res;
                    switch (opt->opt) {
                        case NETOPT_MAC_NO_SLEEP:
                            assert(opt->data_len >= sizeof(netopt_enable_t));
                            *((netopt_enable_t *)opt->data) = ctx.no_sleep;
                            res = sizeof(netopt_enable_t);
                            break;
                        default:
                            /* get option from device driver */
                            res = dev->driver->get(dev, opt->opt, opt->data, opt->data_len);
                            break;
                    }
                    DEBUG("gnrc_contikimac(%d): response of netdev->get: %i\n",
                        thread_getpid(), res);
                    /* send reply to calling thread */
                    msg_t reply = {
                        .type = GNRC_NETAPI_MSG_TYPE_ACK,
                        .content.value = (uint32_t)res,
                    };
                    msg_reply(&msg, &reply);
                    break;
                }
                default:
                    DEBUG("gnrc_contikimac(%d): Unknown command %" PRIu16 "\n",
                        thread_getpid(), msg.type);
                    break;
            }
        }
        if (!ctx.no_sleep && (flags & CONTIKIMAC_THREAD_FLAG_TICK)) {
            gnrc_contikimac_tick(&ctx);
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
