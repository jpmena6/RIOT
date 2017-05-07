/*
 * Copyright (C) 2017 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     drivers_kw41zrf
 * @{
 * @file
 * @brief       Basic functionality of kw41zrf driver
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @}
 */
#include <stdint.h>
#include <string.h>

#include "log.h"
#include "mutex.h"
#include "msg.h"
#include "periph/gpio.h"
#include "periph/cpuid.h"
#include "net/gnrc.h"
#include "net/ieee802154.h"
#include "luid.h"

#include "kw41zrf.h"
#include "kw41zrf_spi.h"
#include "kw41zrf_reg.h"
#include "kw41zrf_netdev.h"
#include "kw41zrf_getset.h"
#include "kw41zrf_intern.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static void kw41zrf_get_mac(kw41zrf_t *dev)
{
    DEBUG("[kw41zrf] Get MAC address\n");
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
    memcpy(&dev->params, params, sizeof(kw41zrf_params_t));
    dev->idle_state = XCVSEQ_RECEIVE;
    dev->state = 0;
    dev->pending_tx = 0;
    kw41zrf_spi_init(dev);
    kw41zrf_set_power_mode(dev, KW2XRF_IDLE);
    DEBUG("[kw41zrf] setup finished\n");
}

int kw41zrf_init(kw41zrf_t *dev, gpio_cb_t cb)
{
    if (dev == NULL) {
        return -ENODEV;
    }

    kw41zrf_set_out_clk(dev);
    kw41zrf_disable_interrupts(dev);
    /* set up GPIO-pin used for IRQ */
    gpio_init_int(dev->params.int_pin, GPIO_IN, GPIO_FALLING, cb, dev);

    kw41zrf_abort_sequence(dev);
    kw41zrf_update_overwrites(dev);
    kw41zrf_timer_init(dev, KW2XRF_TIMEBASE_62500HZ);
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
