/*
 * Copyright (C) 2017 Eistec AB
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2014 PHYTEC Messtechnik GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup        cpu_kw41z KW41Z, KW31Z, KW21Z SiP
 * @ingroup         cpu
 * @brief           CPU specific implementations for the Freescale KW41Z series SiP.
 *                  The SiP incorporates a low power 2.4 GHz transceiver and a
 *                  Kinetis Cortex-M0+ MCU.
 * @{
 *
 * @file
 * @brief           Implementation specific CPU configuration options
 *
 * @author          Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author          Johann Fischer <j.fischer@phytec.de>
 * @author          Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef CPU_CONF_H
#define CPU_CONF_H

#include "cpu_conf_common.h"

#ifdef CPU_MODEL_MKW41Z512VHT4
#include "vendor/MKW41Z4.h"
#else
#error "unknown or undefined CPU_MODEL"
#endif
#include "bme.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   ARM Cortex-M specific CPU configuration
 * @{
 */
#define CPU_DEFAULT_IRQ_PRIO            (1U)
#define CPU_IRQ_NUMOF                   (48U)
#define CPU_FLASH_BASE                  (0x00000000)
/** @} */

/**
 * @name GPIO pin mux function numbers
 */
/** @{ */
#define PIN_MUX_FUNCTION_ANALOG 0
#define PIN_MUX_FUNCTION_GPIO 1
/** @} */
/**
 * @name GPIO interrupt flank settings
 */
/** @{ */
#define PIN_INTERRUPT_RISING  0b1001
#define PIN_INTERRUPT_FALLING 0b1010
#define PIN_INTERRUPT_EDGE    0b1011
/** @} */

/**
 * @name Clock settings for the LPTMR0 timer
 * @{
 */
#define LPTIMER_DEV                      (LPTMR0) /**< LPTIMER hardware module */
#define LPTIMER_CLKEN()                  (bit_set32(&SIM->SCGC5, SIM_SCGC5_LPTMR_SHIFT))    /**< Enable LPTMR0 clock gate */
#define LPTIMER_CLKSRC_MCGIRCLK          0    /**< internal reference clock (4MHz) */
#define LPTIMER_CLKSRC_LPO               1    /**< PMC 1kHz output */
#define LPTIMER_CLKSRC_ERCLK32K          2    /**< RTC clock 32768Hz */
#define LPTIMER_CLKSRC_OSCERCLK          3    /**< system oscillator output, clock from RF-Part */

#ifndef LPTIMER_CLKSRC
#define LPTIMER_CLKSRC                   LPTIMER_CLKSRC_ERCLK32K    /**< default clock source */
#endif

#if (LPTIMER_CLKSRC == LPTIMER_CLKSRC_MCGIRCLK)
#define LPTIMER_CLK_PRESCALE    1
#define LPTIMER_SPEED           1000000
#elif (LPTIMER_CLKSRC == LPTIMER_CLKSRC_OSCERCLK)
#define LPTIMER_CLK_PRESCALE    1
#define LPTIMER_SPEED           1000000
#elif (LPTIMER_CLKSRC == LPTIMER_CLKSRC_ERCLK32K)
#define LPTIMER_CLK_PRESCALE    0
#define LPTIMER_SPEED           32768
#else
#define LPTIMER_CLK_PRESCALE    0
#define LPTIMER_SPEED           1000
#endif
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CPU_CONF_H */
/** @} */
