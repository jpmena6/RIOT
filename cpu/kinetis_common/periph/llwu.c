/*
 * Copyright (C) 2017 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_kinetis_common
 * @{
 *
 * @file
 * @brief       Low-leakage wakeup unit (LLWU) driver
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include "cpu.h"
#include "bit.h"
#include "llwu.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

void llwu_init(void)
{
    /* Setup Low Leakage Wake-up Unit (LLWU) */
#ifdef SIM_SCGC4_LLWU_SHIFT
    /* Not all Kinetis CPUs have a clock gate for the LLWU */
    bit_set32(&SIM->SCGC4, SIM_SCGC4_LLWU_SHIFT);   /* Enable LLWU clock gate */
#endif

    /* Enable LLWU interrupt, or else we can never resume from LLS */
    NVIC_EnableIRQ(LLWU_IRQn);
    /* LLWU needs to have the lowest possible priority, or it will block the
     * other modules from performing their IRQ handling */
    NVIC_SetPriority(LLWU_IRQn, 0xff);

    /* Enable all wakeup modules */
    LLWU->ME = 0xff;
}

void isr_llwu(void)
{
//     uint32_t flags = LLWU->F1 | (LLWU->F2 << 8);
    irq_enable();
    /* Clear LLWU pin interrupt flags */
    LLWU->F1 = LLWU->F1;
    LLWU->F2 = LLWU->F2;
    /* Read only register F3, the flag will need to be cleared in the peripheral
     * instead of writing a 1 to the MWUFx bit. */
    /* LLWU->F3 = 0xFF; */

    DEBUG("LLWU IRQ\n");

    cortexm_isr_end();
}