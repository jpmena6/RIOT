/*
 * Copyright (C) 2018 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     cpu_kinetis
 * @{
 *
 * @file
 * @brief       Default ztimer configuration for Kinetis CPUs
 *
 * This configuration assumes that there is at least one PIT and one LPTMR
 * instance configured in periph_conf.h, and that the first LPTMR instance is
 * configured as a 32.768 kHz timer.
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include "ztimer.h"
#include "ztimer/convert.h"
#include "ztimer/extend.h"
#include "ztimer/periph.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static ztimer_periph_t _ztimer_pit;
static ztimer_periph_t _ztimer_lptmr;
static ztimer_extend_t _ztimer_lptmr_extend;
static ztimer_convert_t _ztimer_lptmr_msec;

#ifdef MODULE_ZTIMER_USEC
__attribute__ ((weak)) ztimer_dev_t *const ZTIMER_USEC = &_ztimer_pit.super;
#endif
#ifdef MODULE_ZTIMER_MSEC
__attribute__ ((weak)) ztimer_dev_t *const ZTIMER_MSEC = &_ztimer_lptmr_msec.super;
#endif
#ifdef MODULE_ZTIMER_LP32K
__attribute__ ((weak)) ztimer_dev_t *const ZTIMER_LP32K = &_ztimer_lptmr_extend.super;
#endif

/* marked as weak to allow any board config to override this function */
__attribute__ ((weak)) void ztimer_board_init(void)
{
#ifdef MODULE_ZTIMER_USEC
    ztimer_periph_init(&_ztimer_pit, TIMER_PIT_DEV(0), 1000000lu);
    _ztimer_pit.adjust = ztimer_diff(&_ztimer_pit.super, 100);
    DEBUG("ztimer_board_init(): ZTIMER_US diff=%"PRIu32"\n", _ztimer_pit.adjust);
#endif

#if defined(MODULE_ZTIMER_MSEC) || defined(MODULE_ZTIMER_LP32K)
    ztimer_periph_init(&_ztimer_lptmr, TIMER_LPTMR_DEV(0), 32768lu);
    ztimer_extend_init(&_ztimer_lptmr_extend, &_ztimer_lptmr.super, 16);
#endif
#ifdef MODULE_ZTIMER_MSEC
    ztimer_convert_init(&_ztimer_lptmr_msec, &_ztimer_lptmr_extend.super, 1000, 32768lu);
#endif
}
