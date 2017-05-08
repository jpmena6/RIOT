/*
 * Copyright (C) 2016 PHYTEC Messtechnik GmbH
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
 * @author      Johann Fischer <j.fischer@phytec.de>
 * @}
 */

#include "panic.h"
#include "kw41zrf.h"
#include "kw41zrf_getset.h"
#include "kw41zrf_intern.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

void kw41zrf_disable_interrupts(kw41zrf_t *dev)
{
    DEBUG("[kw41zrf] disable interrupts\n");
    /* Clear and disable all interrupts */
    ZLL->PHY_CTRL |=
        ZLL_PHY_CTRL_TSM_MSK_MASK |
        ZLL_PHY_CTRL_WAKE_MSK_MASK |
        ZLL_PHY_CTRL_CRC_MSK_MASK |
        ZLL_PHY_CTRL_PLL_UNLOCK_MSK_MASK |
        ZLL_PHY_CTRL_FILTERFAIL_MSK_MASK |
        ZLL_PHY_CTRL_RX_WMRK_MSK_MASK |
        ZLL_PHY_CTRL_CCAMSK_MASK |
        ZLL_PHY_CTRL_RXMSK_MASK |
        ZLL_PHY_CTRL_TXMSK_MASK |
        ZLL_PHY_CTRL_SEQMSK_MASK;

    /* Mask all timer interrupts and clear all interrupt flags */
    ZLL->IRQSTS =
        ZLL_IRQSTS_TMR1MSK_MASK |
        ZLL_IRQSTS_TMR2MSK_MASK |
        ZLL_IRQSTS_TMR3MSK_MASK |
        ZLL_IRQSTS_TMR4MSK_MASK |
        ZLL_IRQSTS_TMR1IRQ_MASK |
        ZLL_IRQSTS_TMR2IRQ_MASK |
        ZLL_IRQSTS_TMR3IRQ_MASK |
        ZLL_IRQSTS_TMR4IRQ_MASK |
        ZLL_IRQSTS_WAKE_IRQ_MASK |
        ZLL_IRQSTS_PLL_UNLOCK_IRQ_MASK |
        ZLL_IRQSTS_FILTERFAIL_IRQ_MASK |
        ZLL_IRQSTS_RXWTRMRKIRQ_MASK |
        ZLL_IRQSTS_CCAIRQ_MASK |
        ZLL_IRQSTS_RXIRQ_MASK |
        ZLL_IRQSTS_TXIRQ_MASK |
        ZLL_IRQSTS_SEQIRQ_MASK;
}
#if 0
void kw41zrf_set_power_mode(kw41zrf_t *dev, kw41zrf_powermode_t pm)
{
    DEBUG("[kw41zrf] set power mode to %d\n", pm);
    uint8_t reg = 0;
    switch (pm) {
        case KW41ZRF_HIBERNATE:
            /* VREG off, XTAL off, Timer off, Current cons. < 1uA */
            reg = 0;
            dev->state = NETOPT_STATE_SLEEP;
            break;

        case KW2XRF_DOZE:
            /* VREG off, XTAL on, Timer on/off, Current cons. 600uA */
            reg = MKW2XDM_PWR_MODES_XTALEN;
            dev->state = NETOPT_STATE_SLEEP;
            break;

        case KW2XRF_IDLE:
            /* VREG on, XTAL on, Timer on, Current cons. 700uA */
            reg = MKW2XDM_PWR_MODES_XTALEN | MKW2XDM_PWR_MODES_PMC_MODE;
            dev->state = NETOPT_STATE_IDLE;
            break;

        case KW2XRF_AUTODOZE:
            reg = MKW2XDM_PWR_MODES_XTALEN | MKW2XDM_PWR_MODES_AUTODOZE;
            dev->state = NETOPT_STATE_IDLE;
            break;
    }
    kw41zrf_write_dreg(dev, MKW2XDM_PWR_MODES, reg);
}
#endif

int kw41zrf_can_switch_to_idle(kw41zrf_t *dev)
{
    uint8_t seq = (ZLL->PHY_CTRL & ZLL_PHY_CTRL_XCVSEQ_MASK) >> ZLL_PHY_CTRL_XCVSEQ_SHIFT;
    uint8_t actual = (ZLL->SEQ_CTRL_STS & ZLL_SEQ_CTRL_STS_XCVSEQ_ACTUAL_MASK) >> ZLL_SEQ_CTRL_STS_XCVSEQ_ACTUAL_SHIFT;
    if (ENABLE_DEBUG) {
        uint8_t state = (ZLL->SEQ_STATE & ZLL_SEQ_STATE_SEQ_STATE_MASK) >> ZLL_SEQ_STATE_SEQ_STATE_SHIFT;
        DEBUG("[kw41zrf] XCVSEQ_ACTUAL=0x%x, XCVSEQ=0x%02x, SEQ_STATE=%x\n", actual, seq, state);
    }

    switch (seq)
    {
        case XCVSEQ_TRANSMIT:
        case XCVSEQ_TX_RX:
            return 0;

        default:
            break;
    }
    switch (actual)
    {
        case XCVSEQ_TRANSMIT:
        case XCVSEQ_TX_RX:
            return 0;

        default:
            break;
    }

    return 1;
}

/** Load the timer value (Setting Current Time) */
static inline void kw41zrf_timer_load(kw41zrf_t *dev, uint32_t value)
{
    (void) dev;
    ZLL->EVENT_TMR = ZLL_EVENT_TMR_EVENT_TMR(value) | ZLL_EVENT_TMR_EVENT_TMR_LD_MASK;
}

static inline uint32_t kw41zrf_timer_get(kw41zrf_t *dev)
{
    (void) dev;
    uint32_t tmp;
    tmp = (ZLL->EVENT_TMR & ZLL_EVENT_TMR_EVENT_TMR_LD_MASK) >> ZLL_EVENT_TMR_EVENT_TMR_SHIFT;
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
    ZLL->TMR_PRESCALE = (ZLL->TMR_PRESCALE & ~ZLL_TMR_PRESCALE_TMR_PRESCALE_MASK) |
        ZLL_TMR_PRESCALE_TMR_PRESCALE(tb);
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
