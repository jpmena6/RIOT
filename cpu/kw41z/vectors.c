/*
 * Copyright (C) 2017 Eistec AB
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2015 PHYTEC Messtechnik GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     cpu_kw41z
 * @{
 *
 * @file
 * @brief       Interrupt vector definition for MKW41Zxxxxxxx MCUs
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Johann Fischer <j.fischer@phytec.de>
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include "vectors_kinetis.h"

/* CPU specific interrupt vector table */
ISR_VECTOR(1) const isr_t vector_cpu[CPU_IRQ_NUMOF] = {
    isr_dma0,              /* DMA channel 0 transfer complete or error */
    isr_dma1,              /* DMA channel 1 transfer complete or error */
    isr_dma2,              /* DMA channel 2 transfer complete or error */
    isr_dma3,              /* DMA channel 3 transfer complete or error */
    dummy_handler,         /* Reserved for future MCM */
    isr_ftfa,              /* FTFA command complete and read collision */
    isr_pmc,               /* PMC: Low-voltage detect, low-voltage warning,
                            * DCDC: PSWITCH interrupt */
    isr_llwu,              /* Low leakage wakeup */
    isr_i2c0,              /* I2C bus 0 interrupt */
    isr_i2c1,              /* I2C bus 1 interrupt */
    isr_spi0,              /* SPI bus 0 interrupt */
    isr_tsi,               /* TSI interrupt */
    isr_lpuart0,           /* LPUART0 interrupt */
    isr_trng0an,           /* TRNG0AN interrupt */
    isr_cmt,               /* Carrier modulator transmitter status or error */
    isr_adc0,              /* Analog-to-digital converter 0 */
    isr_cmp0,              /* Comparator 0 */
    isr_tpm0,              /* Timer/PWM module 0 */
    isr_tpm1,              /* Timer/PWM module 1 */
    isr_tpm2,              /* Timer/PWM module 2 */
    isr_rtc,               /* Real time clock alarm interrupt */
    isr_rtc_seconds,       /* Real time clock seconds seconds interrupt */
    isr_pit,               /* Periodic interrupt timer single vector for all channels */
    isr_ltc,               /* LTC(AESA) */
    isr_radio_int0,        /* Radio transceiver INT0 */
    isr_dac0,              /* Digital-to-analog converter 0 */
    isr_radio_int1,        /* Radio transceiver INT1 */
    isr_mcg,               /* Multipurpose clock generator */
    isr_lptmr0,            /* Low power timer interrupt */
    isr_spi1,              /* SPI bus 1 interrupt */
    isr_porta,             /* Port A pin detect interrupt */
    isr_portbc,            /* Port B and C pin detect interrupt */
};

/* Empty padding space to ensure that all sanity checks in the linking stage are
 * fulfilled. These will be placed in the area between the used vector table
 * starting at memory address 0 and the flash configuration field at 0x400-0x410 */
ISR_VECTOR(2) const isr_t vector_padding[0x100 - 1 - CPU_NONISR_EXCEPTIONS - CPU_IRQ_NUMOF];
