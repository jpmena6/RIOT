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
 * @brief       Internal function of kw41zrf driver
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @}
 */

#include "panic.h"
#include "kw41zrf.h"
#include "kw41zrf_spi.h"
#include "kw41zrf_reg.h"
#include "kw41zrf_getset.h"
#include "kw41zrf_intern.h"
#include "overwrites.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

void kw41zrf_disable_interrupts(kw41zrf_t *dev)
{
    DEBUG("[kw41zrf] disable interrupts\n");
    /* Clear and disable all interrupts */
    kw41zrf_write_dreg(dev, MKW2XDM_PHY_CTRL2, 0xff);
    int reg = kw41zrf_read_dreg(dev, MKW2XDM_PHY_CTRL3);
    reg |= MKW2XDM_PHY_CTRL3_WAKE_MSK | MKW2XDM_PHY_CTRL3_PB_ERR_MSK;
    kw41zrf_write_dreg(dev, MKW2XDM_PHY_CTRL3, reg);

    kw41zrf_write_dreg(dev, MKW2XDM_IRQSTS1, 0x7f);
    kw41zrf_write_dreg(dev, MKW2XDM_IRQSTS2, 0x03);
    kw41zrf_write_dreg(dev, MKW2XDM_IRQSTS3, 0xff);
}

/* update overwrites register */
void kw41zrf_update_overwrites(kw41zrf_t *dev)
{
    kw41zrf_write_dreg(dev, MKW2XDM_OVERWRITE_VER, overwrites_direct[0].data);
    for (uint8_t i = 0; i < sizeof(overwrites_indirect)/sizeof(overwrites_t); i++) {
        kw41zrf_write_iregs(dev, overwrites_indirect[i].address,
                           (uint8_t *)&(overwrites_indirect[i].data), 1);
    }
}

void kw41zrf_set_out_clk(kw41zrf_t *dev)
{
    /* TODO: add clock select */
    /* check modem's crystal oscillator, CLK_OUT shall be 4MHz */
    uint8_t tmp = kw41zrf_read_dreg(dev, MKW2XDM_CLK_OUT_CTRL);
    if (tmp != 0x8Bu) {
        core_panic(PANIC_GENERAL_ERROR, "Could not start MKW2XD radio transceiver");
    }
}

void kw41zrf_set_power_mode(kw41zrf_t *dev, kw41zrf_powermode_t pm)
{
    DEBUG("[kw41zrf] set power mode to %d\n", pm);
    uint8_t reg = 0;
    switch (pm) {
        case kw41zrf_HIBERNATE:
            /* VREG off, XTAL off, Timer off, Current cons. < 1uA */
            reg = 0;
            dev->state = NETOPT_STATE_SLEEP;
            break;

        case kw41zrf_DOZE:
            /* VREG off, XTAL on, Timer on/off, Current cons. 600uA */
            reg = MKW2XDM_PWR_MODES_XTALEN;
            dev->state = NETOPT_STATE_SLEEP;
            break;

        case kw41zrf_IDLE:
            /* VREG on, XTAL on, Timer on, Current cons. 700uA */
            reg = MKW2XDM_PWR_MODES_XTALEN | MKW2XDM_PWR_MODES_PMC_MODE;
            dev->state = NETOPT_STATE_IDLE;
            break;

        case kw41zrf_AUTODOZE:
            reg = MKW2XDM_PWR_MODES_XTALEN | MKW2XDM_PWR_MODES_AUTODOZE;
            dev->state = NETOPT_STATE_IDLE;
            break;
    }
    kw41zrf_write_dreg(dev, MKW2XDM_PWR_MODES, reg);
}

int kw41zrf_can_switch_to_idle(kw41zrf_t *dev)
{
    uint8_t state = kw41zrf_read_dreg(dev, MKW2XDM_SEQ_STATE);
    uint8_t seq = kw41zrf_read_dreg(dev, MKW2XDM_PHY_CTRL1) & MKW2XDM_PHY_CTRL1_XCVSEQ_MASK;
    DEBUG("[kw41zrf] state 0x%0x, seq 0x%0x\n", state, seq);

    if ((seq != XCVSEQ_TRANSMIT) && (seq != XCVSEQ_TX_RX)) {
        return 1;
    }

    if (state != 0) {
        return 0;
    }

    return 0;
}

/** Load the timer value (Setting Current Time) */
static void kw41zrf_timer_load(kw41zrf_t *dev, uint32_t value)
{
    kw41zrf_write_dregs(dev, MKW2XDM_T1CMP_LSB, (uint8_t *)&value, sizeof(value));
    kw41zrf_set_dreg_bit(dev, MKW2XDM_PHY_CTRL4, MKW2XDM_PHY_CTRL4_TMRLOAD);
}

static uint32_t kw41zrf_timer_get(kw41zrf_t *dev)
{
    uint32_t tmp;
    kw41zrf_read_dregs(dev, MKW2XDM_EVENT_TIMER_LSB, (uint8_t*)&tmp, sizeof(tmp));
    return tmp;
}

/** Set an absolute timeout value for the given compare register of the Event Timer */
static void kw41zrf_timer_set_absolute(kw41zrf_t *dev, uint8_t cmp_reg, uint32_t value)
{
    kw41zrf_write_dregs(dev, cmp_reg, (uint8_t *)&value, 3);
}

/** Set an timeout value for the given compare register of the Event Timer */
static void kw41zrf_timer_set(kw41zrf_t *dev, uint8_t cmp_reg, uint32_t timeout)
{
    uint32_t now = kw41zrf_timer_get(dev);

    DEBUG("[kw41zrf] timer now: %" PRIx32 ", set %" PRIx32 "\n", now, now + timeout);
    kw41zrf_timer_set_absolute(dev, cmp_reg, now + timeout);
}

void kw41zrf_timer_init(kw41zrf_t *dev, kw41zrf_timer_timebase_t tb)
{
    uint8_t tmp = MKW2XDMI_TMR_PRESCALE_SET(tb);

    kw41zrf_write_iregs(dev, MKW2XDMI_TMR_PRESCALE, &tmp, 1);
    kw41zrf_timer_load(dev, 0);
}

void kw41zrf_timer2_seq_start_on(kw41zrf_t *dev)
{
    kw41zrf_set_dreg_bit(dev, MKW2XDM_PHY_CTRL1, MKW2XDM_PHY_CTRL1_TMRTRIGEN);
}

void kw41zrf_timer2_seq_start_off(kw41zrf_t *dev)
{
    kw41zrf_clear_dreg_bit(dev, MKW2XDM_PHY_CTRL1, MKW2XDM_PHY_CTRL1_TMRTRIGEN);
}

void kw41zrf_timer3_seq_abort_on(kw41zrf_t *dev)
{
    kw41zrf_set_dreg_bit(dev, MKW2XDM_PHY_CTRL4, MKW2XDM_PHY_CTRL4_TC3TMOUT);
}

void kw41zrf_timer3_seq_abort_off(kw41zrf_t *dev)
{
    kw41zrf_clear_dreg_bit(dev, MKW2XDM_PHY_CTRL4, MKW2XDM_PHY_CTRL4_TC3TMOUT);
}

void kw41zrf_trigger_tx_ops_enable(kw41zrf_t *dev, uint32_t timeout)
{
    kw41zrf_timer_set(dev, MKW2XDM_T2CMP_LSB, timeout);
    kw41zrf_set_dreg_bit(dev, MKW2XDM_PHY_CTRL3, MKW2XDM_PHY_CTRL3_TMR2CMP_EN);
}

void kw41zrf_trigger_tx_ops_disable(kw41zrf_t *dev)
{
    kw41zrf_clear_dreg_bit(dev, MKW2XDM_PHY_CTRL3, MKW2XDM_PHY_CTRL3_TMR2CMP_EN);
    kw41zrf_write_dreg(dev, MKW2XDM_IRQSTS3, MKW2XDM_IRQSTS3_TMR2IRQ);
    DEBUG("[kw41zrf] trigger_tx_ops_disable, now: %" PRIx32 "\n", kw41zrf_timer_get(dev));
}

void kw41zrf_abort_rx_ops_enable(kw41zrf_t *dev, uint32_t timeout)
{
    kw41zrf_timer_set(dev, MKW2XDM_T3CMP_LSB, timeout);
    kw41zrf_set_dreg_bit(dev, MKW2XDM_PHY_CTRL3, MKW2XDM_PHY_CTRL3_TMR3CMP_EN);
}

void kw41zrf_abort_rx_ops_disable(kw41zrf_t *dev)
{
    kw41zrf_clear_dreg_bit(dev, MKW2XDM_PHY_CTRL3, MKW2XDM_PHY_CTRL3_TMR3CMP_EN);
    kw41zrf_write_dreg(dev, MKW2XDM_IRQSTS3, MKW2XDM_IRQSTS3_TMR3IRQ);
    DEBUG("[kw41zrf] abort_rx_ops_disable, now: %" PRIx32 "\n", kw41zrf_timer_get(dev));
}

void kw41zrf_seq_timeout_on(kw41zrf_t *dev, uint32_t timeout)
{
    kw41zrf_mask_irq_b(dev);
    kw41zrf_timer_set(dev, MKW2XDM_T4CMP_LSB, timeout);

    /* enable and clear irq for timer 3 */
    uint8_t irqsts3 = kw41zrf_read_dreg(dev, MKW2XDM_IRQSTS3) & 0xf0;
    irqsts3 &= ~MKW2XDM_IRQSTS3_TMR4MSK;
    irqsts3 |= MKW2XDM_IRQSTS3_TMR4IRQ;
    kw41zrf_write_dreg(dev, MKW2XDM_IRQSTS3, irqsts3);

    kw41zrf_set_dreg_bit(dev, MKW2XDM_PHY_CTRL3, MKW2XDM_PHY_CTRL3_TMR4CMP_EN);
    kw41zrf_enable_irq_b(dev);
}

void kw41zrf_seq_timeout_off(kw41zrf_t *dev)
{
    kw41zrf_clear_dreg_bit(dev, MKW2XDM_PHY_CTRL3, MKW2XDM_PHY_CTRL3_TMR4CMP_EN);
    kw41zrf_write_dreg(dev, MKW2XDM_IRQSTS3, MKW2XDM_IRQSTS3_TMR4IRQ);
    DEBUG("[kw41zrf] seq_timeout_off, now: %" PRIx32 "\n", kw41zrf_timer_get(dev));
}

uint32_t kw41zrf_get_timestamp(kw41zrf_t *dev)
{
    uint32_t tmp;
    kw41zrf_read_dregs(dev, MKW2XDM_TIMESTAMP_LSB, (uint8_t*)&tmp, sizeof(tmp));
    return tmp;
}
