/*
 * Copyright (C) 2017 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup    cpu_kinetis_common_llwu Kinetis LLWU
 * @ingroup     cpu_kinetis_common
 * @brief       Kinetis low leakage wakeup unit (LLWU) driver

 * @{
 *
 * @file
 * @brief       Interface definition for the Kinetis LLWU driver.
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef LLWU_H
#define LLWU_H

#include "cpu.h"
#include "bit.h"
#include "periph_conf.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize the LLWU hardware
 */
void llwu_init(void);

/**
 * @brief Enable a wakeup module in the LLWU
 */
inline static void llwu_wakeup_module_enable(llwu_wakeup_module_t mod)
{
    assert(mod < LLWU_WAKEUP_MODULE_NUMOF);
    bit_set8(&LLWU->ME, mod);
}

/**
 * @brief Disable a wakeup module in the LLWU
 */
inline static void llwu_wakeup_module_disable(llwu_wakeup_module_t mod)
{
    assert(mod < LLWU_WAKEUP_MODULE_NUMOF);
    bit_clear8(&LLWU->ME, mod);
}

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* LLWU_H */
