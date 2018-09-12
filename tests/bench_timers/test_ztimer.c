/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Another peripheral timer test application
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include <stddef.h>
#include <stdint.h>
#include "board.h"
#include "cpu.h"
#include "periph/timer.h"

#include "thread_flags.h"
#include "fmt.h"
#include "ztimer.h"
#include "print_results.h"
#include "spin_random.h"
#include "bench_timers_config.h"

#ifndef TEST_TRACE
#define TEST_TRACE 0
#endif

enum test_xtimer_variants {
    TEST_ZTIMER_SET             = 0,
    TEST_PARALLEL               = 1,
};

const result_presentation_t test_ztimer_presentation = {
    .groups = (const result_group_t[1]) {
        {
            .label = "ztimer",
            .sub_labels = (const char *[]){
                [TEST_ZTIMER_SET] = "zt_set",
                [TEST_ZTIMER_SET | TEST_PARALLEL] = "zt_set race",
            },
            .num_sub_labels = TEST_VARIANT_NUMOF,
        },
    },
    .num_groups = 1,
    .ref_limits = &bench_timers_ref_limits,
    .int_limits = &bench_timers_int_limits,
    .offsets = (const unsigned[]) {
        [TEST_ZTIMER_SET]                               = TEST_MIN_REL,
        [TEST_ZTIMER_SET | TEST_PARALLEL]               = TEST_MIN_REL,
    },
};


static void cb_nop(void *arg)
{
    (void)arg;
}

void test_ztimer_run(test_ctx_t *ctx, uint32_t interval, unsigned int variant)
{
    interval += TEST_MIN;
    unsigned int interval_ref = TIM_TEST_TO_REF(interval);
    ztimer_t zt = {
        .callback = bench_timers_cb,
        .arg = ctx,
    };
    ztimer_t zt_parallel = {
        .callback = cb_nop,
        .arg = NULL,
    };
    if (TEST_TRACE) {
        switch (variant & ~TEST_PARALLEL) {
            case TEST_ZTIMER_SET:
                print_str("rel ");
                break;
            default:
                break;
        }
        if (variant & TEST_PARALLEL) {
            print_str("= ");
        }
        else {
            print_str("- ");
        }
        print_u32_dec(interval);
        print("\n", 1);
    }

    spin_random_delay();
    if (variant & TEST_PARALLEL) {
        ztimer_set(TEST_ZTIMER_DEV, &zt_parallel, interval);
        spin_random_delay();
    }
    ctx->target_ref = timer_read(TIM_REF_DEV) + interval_ref;
    uint32_t now = TUT_READ();
    ctx->target_tut = now + interval;
    switch (variant & ~TEST_PARALLEL) {
        case TEST_ZTIMER_SET:
            ztimer_set(TEST_ZTIMER_DEV, &zt, interval);
            break;
        default:
            break;
    }
    thread_flags_wait_any(THREAD_FLAG_TEST);
    ztimer_remove(TEST_ZTIMER_DEV, &zt_parallel);
    ztimer_remove(TEST_ZTIMER_DEV, &zt);
}
