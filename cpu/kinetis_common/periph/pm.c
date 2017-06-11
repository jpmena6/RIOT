/*
 * Copyright (C) 2016 Kaspar Schleiser <kaspar@schleiser.de>
 *               2014 Eistec AB
 *               2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_kinetis_common
 * @ingroup     drivers_periph_pm
 * @{
 *
 * @file
 * @brief       Implementation of the kernels power management interface
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 *
 * @}
 */

#include "periph/pm.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/* SMC_PMCTRL_STOPM masks */
enum {
    SMC_PMCTRL_STOPM_STOP = 0,
    /* 1 is reserved */
    SMC_PMCTRL_STOPM_VLPS = 2,
    SMC_PMCTRL_STOPM_LLS  = 3,
    /* VLLS is not supported */
};

/** Configure which stop mode will be entered when cortexm_sleep(1) is called */
static inline void pm_stopm(uint8_t stopm)
{
    SMC->PMCTRL = (SMC->PMCTRL & ~(SMC_PMCTRL_STOPM_MASK)) | SMC_PMCTRL_STOPM(stopm);
}

void pm_set(unsigned mode)
{
    unsigned deep = 1;
    switch (mode) {
        case KINETIS_PM_WAIT:
            /* WAIT */
            deep = 0;
            break;
        case KINETIS_PM_STOP:
            /* STOP */
            pm_stopm(SMC_PMCTRL_STOPM_STOP);
            break;
        case KINETIS_PM_VLPS:
            /* VLPS */
            pm_stopm(SMC_PMCTRL_STOPM_VLPS);
            break;
        case KINETIS_PM_LLS:
            /* LLSx */
//             pm_stopm(SMC_PMCTRL_STOPM_VLPS);
            pm_stopm(SMC_PMCTRL_STOPM_LLS);
            break;
    }
    DEBUG("pm_set(%u)\n", mode);
    cortexm_sleep(deep);
}
