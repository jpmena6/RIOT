/*
 * Copyright (C) 2015-2017 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup        cpu_k60 NXP Kinetis K60
 * @ingroup         cpu
 * @brief           CPU specific implementations for the NXP Kinetis K60
 *                  Cortex-M4 MCU
 * @{
 *
 * @file
 * @brief           Implementation specific CPU configuration options
 *
 * @author          Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef CPU_CONF_H
#define CPU_CONF_H

#if defined(CPU_MODEL_MK60DN512VLL10) || defined(CPU_MODEL_MK60DN256VLL10)
#include "vendor/MK60D10.h"

/* K60 rev 2.x replaced the RNG module in 1.x by the RNGA PRNG module */
#define KINETIS_RNGA            (RNG)
#else
#error Unknown CPU model. Update Makefile.include in the board directory.
#endif

#include "cpu_conf_kinetis.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief This CPU provides an additional ADC clock divider as CFG1[ADICLK]=1
 */
#define KINETIS_HAVE_ADICLK_BUS_DIV_2 1

/**
 * @brief Internal modules whose interrupts are mapped to LLWU wake up sources.
 *
 * Other modules CAN NOT be used to wake the CPU from LLS or VLLSx power modes.
 */
typedef enum llwu_wakeup_module {
    LLWU_WAKEUP_MODULE_LPTMR0 = 0,
    LLWU_WAKEUP_MODULE_CMP0 = 1,
    LLWU_WAKEUP_MODULE_CMP1 = 2,
    LLWU_WAKEUP_MODULE_CMP2 = 3,
    LLWU_WAKEUP_MODULE_TSI = 4,
    LLWU_WAKEUP_MODULE_RTC_ALARM = 5,
    LLWU_WAKEUP_MODULE_RESERVED = 6,
    LLWU_WAKEUP_MODULE_RTC_SECONDS = 7,
    LLWU_WAKEUP_MODULE_NUMOF
} llwu_wakeup_module_t;

/**
 * @brief enum that maps physical pins to wakeup pin numbers in LLWU module
 *
 * Other pins CAN NOT be used to wake the CPU from LLS or VLLSx power modes.
 */
typedef enum llwu_wakeup_pin {
    LLWU_WAKEUP_PIN_PTE1 = 0,
    LLWU_WAKEUP_PIN_PTE2 = 1,
    LLWU_WAKEUP_PIN_PTE4 = 2,
    LLWU_WAKEUP_PIN_PTA4 = 3,
    LLWU_WAKEUP_PIN_PTA13 = 4,
    LLWU_WAKEUP_PIN_PTB0 = 5,
    LLWU_WAKEUP_PIN_PTC1 = 6,
    LLWU_WAKEUP_PIN_PTC3 = 7,
    LLWU_WAKEUP_PIN_PTC4 = 8,
    LLWU_WAKEUP_PIN_PTC5 = 9,
    LLWU_WAKEUP_PIN_PTC6 = 10,
    LLWU_WAKEUP_PIN_PTC11 = 11,
    LLWU_WAKEUP_PIN_PTD0 = 12,
    LLWU_WAKEUP_PIN_PTD2 = 13,
    LLWU_WAKEUP_PIN_PTD4 = 14,
    LLWU_WAKEUP_PIN_PTD6 = 15,
    LLWU_WAKEUP_PIN_NUMOF
} llwu_wakeup_pin_t;

#ifdef __cplusplus
}
#endif

#endif /* CPU_CONF_H */
/** @} */
