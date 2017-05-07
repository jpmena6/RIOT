/*
 * Copyright (C) 2017 Linaro Limited
 * Copyright (C) 2017 SKF AB
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @ingroup     drivers_kw41zrf
 * @{
 * @file
 * @brief       Basic functionality of kw41zrf driver
 *
 * This implementation is based on the KW41Z IEEE802.15.4 driver in Zephyr OS
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @}
 */
#include <stdint.h>
#include <string.h>

#include "log.h"
#include "mutex.h"
#include "msg.h"
#include "net/gnrc.h"
#include "net/ieee802154.h"
#include "luid.h"

#include "kw41zrf.h"
#include "kw41zrf_netdev.h"
#include "kw41zrf_getset.h"
#include "kw41zrf_intern.h"
#include "fsl_xcvr.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

enum {
    KW41Z_CCA_ED,
    KW41Z_CCA_MODE1,
    KW41Z_CCA_MODE2,
    KW41Z_CCA_MODE3
};

enum {
    KW41Z_STATE_IDLE,
    KW41Z_STATE_RX,
    KW41Z_STATE_TX,
    KW41Z_STATE_CCA,
    KW41Z_STATE_TXRX,
    KW41Z_STATE_CCCA
};

struct kw41z_context {
    struct net_if *iface;
    u8_t mac_addr[8];

    struct k_sem seq_sync;
    atomic_t seq_retval;

    u32_t rx_warmup_time;
    u32_t tx_warmup_time;

    u8_t lqi;
};

static struct kw41z_context kw41z_context_data;

static void kw41zrf_set_address(kw41zrf_t *dev)
{
    DEBUG("[kw41zrf] Set MAC address\n");
    eui64_t addr_long;
    /* get an 8-byte unique ID to use as hardware address */
    luid_get(addr_long.uint8, IEEE802154_LONG_ADDRESS_LEN);
    /* make sure we mark the address as non-multicast and not globally unique */
    addr_long.uint8[0] &= ~(0x01);
    addr_long.uint8[0] |=  (0x02);
    /* set short and long address */
    kw41zrf_set_addr_long(dev, ntohll(addr_long.uint64.u64));
    kw41zrf_set_addr_short(dev, ntohs(addr_long.uint16[0].u16));
}

void kw41zrf_setup(kw41zrf_t *dev, const kw41zrf_params_t *params)
{
    netdev_t *netdev = (netdev_t *)dev;

    netdev->driver = &kw41zrf_driver;
    /* initialize device descriptor */
    dev->idle_state = XCVSEQ_RECEIVE;
    dev->state = 0;
    dev->pending_tx = 0;
    kw41zrf_set_power_mode(dev, KW41ZRF_IDLE);
    DEBUG("[kw41zrf] setup finished\n");
}

int kw41zrf_init(kw41zrf_t *dev, gpio_cb_t cb)
{
    if (dev == NULL) {
        return -ENODEV;
    }

    xcvrStatus_t xcvrStatus = XCVR_Init(ZIGBEE_MODE, DR_500KBPS);
    if (xcvrStatus != gXcvrSuccess_c) {
        return -EIO;
    }

    /* Disable all timers, enable AUTOACK, mask all interrupts */
    ZLL->PHY_CTRL = ZLL_PHY_CTRL_CCATYPE(KW41Z_CCA_MODE1)	|
    ZLL_IRQSTS_WAKE_IRQ_MASK		|
    ZLL_PHY_CTRL_CRC_MSK_MASK		|
    ZLL_PHY_CTRL_PLL_UNLOCK_MSK_MASK	|
    ZLL_PHY_CTRL_FILTERFAIL_MSK_MASK	|
    ZLL_PHY_CTRL_CCAMSK_MASK		|
    ZLL_PHY_CTRL_RXMSK_MASK			|
    ZLL_PHY_CTRL_TXMSK_MASK			|
    ZLL_PHY_CTRL_SEQMSK_MASK;
#if KW41Z_AUTOACK_ENABLED
    ZLL->PHY_CTRL |= ZLL_PHY_CTRL_AUTOACK_MASK;
#endif

    /*
     * Clear all PP IRQ bits to avoid unexpected interrupts immediately
     * after init disable all timer interrupts
     */
    ZLL->IRQSTS = ZLL->IRQSTS;

    /* Clear HW indirect queue */
    ZLL->SAM_TABLE |= ZLL_SAM_TABLE_INVALIDATE_ALL_MASK;

    /* Accept FrameVersion 0 and 1 packets, reject all others */
    ZLL->PHY_CTRL &= ~ZLL_PHY_CTRL_PROMISCUOUS_MASK;
    ZLL->RX_FRAME_FILTER &= ~ZLL_RX_FRAME_FILTER_FRM_VER_FILTER_MASK;
    ZLL->RX_FRAME_FILTER = ZLL_RX_FRAME_FILTER_FRM_VER_FILTER(3)	|
    ZLL_RX_FRAME_FILTER_CMD_FT_MASK		|
    ZLL_RX_FRAME_FILTER_DATA_FT_MASK		|
    ZLL_RX_FRAME_FILTER_BEACON_FT_MASK;

    /* Set prescaller to obtain 1 symbol (16us) timebase */
    ZLL->TMR_PRESCALE = 0x05;

    kw41z_tmr2_disable();
    kw41z_tmr1_disable();

    /* Compute warmup times (scaled to 16us) */
    kw41z->rx_warmup_time =
        (XCVR_TSM->END_OF_SEQ & XCVR_TSM_END_OF_SEQ_END_OF_RX_WU_MASK) >>
        XCVR_TSM_END_OF_SEQ_END_OF_RX_WU_SHIFT;
    kw41z->tx_warmup_time =
        (XCVR_TSM->END_OF_SEQ & XCVR_TSM_END_OF_SEQ_END_OF_TX_WU_MASK) >>
        XCVR_TSM_END_OF_SEQ_END_OF_TX_WU_SHIFT;

    if (kw41z->rx_warmup_time & 0x0F) {
        kw41z->rx_warmup_time = 1 + (kw41z->rx_warmup_time >> 4);
    } else {
        kw41z->rx_warmup_time = kw41z->rx_warmup_time >> 4;
    }

    if (kw41z->tx_warmup_time & 0x0F) {
        kw41z->tx_warmup_time = 1 + (kw41z->tx_warmup_time >> 4);
    } else {
        kw41z->tx_warmup_time = kw41z->tx_warmup_time >> 4;
    }

    /* Set CCA threshold to -75 dBm */
    ZLL->CCA_LQI_CTRL &= ~ZLL_CCA_LQI_CTRL_CCA1_THRESH_MASK;
    ZLL->CCA_LQI_CTRL |= ZLL_CCA_LQI_CTRL_CCA1_THRESH(0xB5);

    /* Set the default power level */
    kw41z_set_txpower(dev, 0);

    /* Adjust ACK delay to fulfill the 802.15.4 turnaround requirements */
    ZLL->ACKDELAY &= ~ZLL_ACKDELAY_ACKDELAY_MASK;
    ZLL->ACKDELAY |= ZLL_ACKDELAY_ACKDELAY(-8);

    /* Adjust LQI compensation */
    ZLL->CCA_LQI_CTRL &= ~ZLL_CCA_LQI_CTRL_LQI_OFFSET_COMP_MASK;
    ZLL->CCA_LQI_CTRL |= ZLL_CCA_LQI_CTRL_LQI_OFFSET_COMP(96);

    /* Set default channel to 2405 MHZ */
    kw41z_set_channel(dev, KW41Z_DEFAULT_CHANNEL);

    /* Unmask Transceiver Global Interrupts */
    ZLL->PHY_CTRL &= ~ZLL_PHY_CTRL_TRCV_MSK_MASK;

    /* Configre Radio IRQ */
    NVIC_ClearPendingIRQ(Radio_1_IRQn);
    IRQ_CONNECT(Radio_1_IRQn, RADIO_0_IRQ_PRIO, kw41z_isr, 0, 0);

    kw41zrf_abort_sequence(dev);
    kw41zrf_update_overwrites(dev);
    kw41zrf_timer_init(dev, KW2XRF_TIMEBASE_62500HZ);*/
    DEBUG("[kw41zrf] init finished\n");

    return 0;
}

void kw41zrf_reset_phy(kw41zrf_t *dev)
{
    /* reset options and sequence number */
    dev->netdev.seq = 0;
    dev->netdev.flags = 0;

    /* set default protocol */
#ifdef MODULE_GNRC_SIXLOWPAN
    dev->netdev.proto = GNRC_NETTYPE_SIXLOWPAN;
#elif MODULE_GNRC
    dev->netdev.proto = GNRC_NETTYPE_UNDEF;
#endif

    dev->tx_power = KW2XRF_DEFAULT_TX_POWER;
    kw41zrf_set_tx_power(dev, dev->tx_power);

    kw41zrf_set_channel(dev, KW2XRF_DEFAULT_CHANNEL);

    kw41zrf_set_pan(dev, KW2XRF_DEFAULT_PANID);
    kw41zrf_set_address(dev);

    kw41zrf_set_cca_mode(dev, 1);

    kw41zrf_set_rx_watermark(dev, 1);

    kw41zrf_set_option(dev, KW2XRF_OPT_AUTOACK, true);
    kw41zrf_set_option(dev, KW2XRF_OPT_ACK_REQ, true);
    kw41zrf_set_option(dev, KW2XRF_OPT_AUTOCCA, true);

    kw41zrf_set_power_mode(dev, KW2XRF_AUTODOZE);
    kw41zrf_set_sequence(dev, dev->idle_state);

    kw41zrf_set_option(dev, KW2XRF_OPT_TELL_RX_START, true);
    kw41zrf_set_option(dev, KW2XRF_OPT_TELL_RX_END, true);
    kw41zrf_set_option(dev, KW2XRF_OPT_TELL_TX_END, true);
    kw41zrf_clear_dreg_bit(dev, MKW2XDM_PHY_CTRL2, MKW2XDM_PHY_CTRL2_SEQMSK);

    kw41zrf_enable_irq_b(dev);

    DEBUG("[kw41zrf] init phy and (re)set to channel %d and pan %d.\n",
          KW2XRF_DEFAULT_CHANNEL, KW2XRF_DEFAULT_PANID);
}
