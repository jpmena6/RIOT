/*
 * Copyright (C) 2017 Eistec AB
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
 * @brief       Implementation of the KW41Z CPU initialization
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @}
 */

#include <stdint.h>
#include "cpu.h"
#include "mcg.h"
#include "cpu_conf.h"
#include "periph/init.h"

static void cpu_clock_init(void);

/**
 * @brief Initialize the CPU, set IRQ priorities
 */
void cpu_init(void)
{
    /* initialize the Cortex-M core */
    cortexm_init();
    /* initialize the clock system */
    cpu_clock_init();
    /* trigger static peripheral initialization */
    periph_init();
}

/**
 * @brief Configure the clock prescalers
 *
 * | Clock name | Run mode frequency (max) | VLPR mode frequency (max) |
 *
 * | Core       |  48 MHz                  |   4 MHz                   |
 * | System     |  48 MHz                  |   4 MHz                   |
 * | Bus        |  24 MHz                  |   1 MHz or 800 KHz        |
 * | Flash      |  24 MHz                  |   1 MHz or 800 KHz        |
 */
static void cpu_clock_init(void)
{
    /* setup system prescalers */
    /* TODO: Make this configurable */
    SIM->CLKDIV1 = (uint32_t)SIM_CLKDIV1_OUTDIV4(1);
}
