/*
 * Copyright (C) 2017 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup        cpu_kw41z NXP Kinetis KW41Z, KW31Z, KW21Z
 * @ingroup         cpu
 * @brief           CPU specific definitions for the NXP Kinetis KW41Z series SoC
 *
 *                  The SoC incorporates a low power 2.4 GHz transceiver and a
 *                  Kinetis Cortex-M0+ MCU.
 * @{
 *
 * @file
 * @brief           Implementation specific CPU configuration options
 *
 * @author          Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef CPU_CONF_H
#define CPU_CONF_H

#if defined(CPU_MODEL_MKW41Z512VHT4) || defined(CPU_MODEL_MKW41Z256VHT4)
#include "vendor/MKW41Z4.h"
#elif defined(CPU_MODEL_MKW31Z512VHT4) || defined(CPU_MODEL_MKW31Z256VHT4)
#include "vendor/MKW31Z4.h"
#elif defined(CPU_MODEL_MKW21Z512VHT4) || defined(CPU_MODEL_MKW21Z256VHT4)
#include "vendor/MKW21Z4.h"
#else
#error "unknown or undefined CPU_MODEL"
#endif

#include "cpu_conf_kinetis.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Internal modules whose interrupts are mapped to LLWU wake up sources.
 *
 * Other modules CAN NOT be used to wake the CPU from LLS or VLLSx power modes.
 */
typedef enum llwu_wakeup_module {
    LLWU_WAKEUP_MODULE_LPTMR0 = 0,
    LLWU_WAKEUP_MODULE_CMP0 = 1,
    LLWU_WAKEUP_MODULE_RADIO = 2,
    LLWU_WAKEUP_MODULE_DCDC = 3,
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
    LLWU_WAKEUP_PIN_PTC16 =  0,
    LLWU_WAKEUP_PIN_PTC17 =  1,
    LLWU_WAKEUP_PIN_PTC18 =  2,
    LLWU_WAKEUP_PIN_PTC19 =  3,
    LLWU_WAKEUP_PIN_PTA16 =  4,
    LLWU_WAKEUP_PIN_PTA17 =  5,
    LLWU_WAKEUP_PIN_PTA18 =  6,
    LLWU_WAKEUP_PIN_PTA19 =  7,
    LLWU_WAKEUP_PIN_PTB0  =  8,
    LLWU_WAKEUP_PIN_PTC0  =  9,
    LLWU_WAKEUP_PIN_PTC2  = 10,
    LLWU_WAKEUP_PIN_PTC3  = 11,
    LLWU_WAKEUP_PIN_PTC4  = 12,
    LLWU_WAKEUP_PIN_PTC5  = 13,
    LLWU_WAKEUP_PIN_PTC6  = 14,
    LLWU_WAKEUP_PIN_PTC7  = 15,
    LLWU_WAKEUP_PIN_NUMOF
} llwu_wakeup_pin_t;

#ifdef __cplusplus
}
#endif

#endif /* CPU_CONF_H */
/** @} */
