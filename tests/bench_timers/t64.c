/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     sys_t64
 *
 * @{
 *
 * @file
 * @brief       T64 timer implementation
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include "t64.h"
#include <stdint.h>
#include "assert.h"
#include "irq.h"
#include "periph/timer.h"
#include "bench_periph_timer_config.h"

#define ENABLE_DEBUG 1
#include "debug.h"
#include "fmt.h"

#ifndef T64_DEV
#define T64_DEV (TIMER_DEV(0))
#endif
#ifndef T64_CHAN
#define T64_CHAN (0)
#endif
/* Maximum settable timeout for the lower level timer */
#ifndef T64_LOWER_MAX
#define T64_LOWER_MAX (0xffffffff)
#endif
/* Partition size, must be a power of two */
#define T64_PARTITION (((T64_LOWER_MAX) >> 15) + 1)
/* in-partition volatile bits */
#define T64_PARTITION_MASK ((T64_PARTITION - 1))
/* Minimum relative timeout, used when the target has been missed */
#define T64_MIN_REL 1

#ifndef T64_TRACE
#define T64_TRACE   0
#endif

typedef struct {
    uint64_t base;
    uint64_t target;
    t64_cb_t cb;
    void *arg;
    unsigned int lower_partition;
    unsigned int awaiting_overflow;
    unsigned int needs_update;
} t64_state_t;

t64_state_t t64_state;

/**
 * @brief   Check for partition transitions and update base accordingly
 */
void t64_update_partition(unsigned int now)
{
    if ((now ^ t64_state.lower_partition) >= (T64_PARTITION)) {
        /* timer count has passed from one partition to the next, update base */
        t64_state.base += (T64_PARTITION);
        t64_state.lower_partition = (t64_state.lower_partition + (T64_PARTITION)) & (T64_LOWER_MAX);
        if (T64_TRACE) {
            print_str("next ");
            print_u32_hex(now);
            print_str(" ");
            print_u32_hex(t64_state.lower_partition);
            print_str(" ");
            print_u64_hex(t64_state.base);
            print("\n", 1);
        }
        assert((now ^ t64_state.lower_partition) < (T64_PARTITION));
    }
}

/**
 * @brief   Set next low level timer timeout and update base if necessary
 *
 * This will set the real target timer if it is within the same partition as the
 * current time, or set an overflow timeout otherwise.
 *
 * @pre IRQ disabled
 */
void t64_update_timeouts(unsigned int before)
{
    /* Keep trying until we manage to set a timer */
    while(1) {
        t64_update_partition(before);
        if (!t64_state.needs_update) {
            break;
        }
        if ((t64_state.target != 0) && (t64_state.target <= (t64_state.base + (before & (T64_PARTITION_MASK))))) {
            if (T64_TRACE) {
                print_str("<<<z ");
                print_u64_hex(t64_state.target);
                print_str(" ");
                print_u64_hex(t64_state.base + (before & (T64_PARTITION_MASK)));
                print_str(" ");
                print_u64_hex(t64_state.base);
                print_str("\n");
            }
            /* Target is in the past */
            t64_state.awaiting_overflow = 0;
            /* We can't run the callback immediately because we want to enforce
             * the masked interrupts */
            timer_set(T64_DEV, T64_CHAN, T64_MIN_REL);
        }
        else if ((t64_state.target == 0) ||
            ((t64_state.base ^ t64_state.target) >= (T64_PARTITION))) {
            /* Timer partition overflow will occur before the target time, or no target set */
            t64_state.awaiting_overflow = 1;
            /* There is a danger of setting an absolute timer target in the low
             * level timer since we might run past the target before the timer
             * has been updated with the new target time */
            unsigned int lower_target = t64_state.lower_partition | (T64_PARTITION_MASK);
            timer_set_absolute(T64_DEV, (T64_CHAN),
                lower_target);
            unsigned int after = timer_read(T64_DEV);
            if (T64_TRACE) {
                print_str("part ");
                print_u32_hex(before);
                print_str(" ");
                print_u32_hex(after);
                print_str(" ");
                print_u32_hex(lower_target);
                print_str(" ");
                print_u32_hex(t64_state.lower_partition);
                print_str(" ");
                print_u64_hex(t64_state.base);
                print_str(" ");
                print_u64_hex(t64_state.target);
                print_str("\n");
            }
            if ((before ^ after) >= (T64_PARTITION)) {
                /* Partition transition occurred while setting the timeout, abort and retry */
                timer_clear(T64_DEV, T64_CHAN);
                before = after;
                /* try again */
                continue;
            }
        }
        else {
            /* Set real target */
            t64_state.awaiting_overflow = 0;
            /* discard top bits and compute lower timer target phase */
            unsigned int lower_target = ((unsigned int)t64_state.target) - ((unsigned int)t64_state.base);
            unsigned int timeout = (lower_target - before) & (T64_PARTITION_MASK);
            timer_set(T64_DEV, T64_CHAN, timeout);
            if (T64_TRACE) {
                print_str("targ ");
                print_u32_hex(before);
                print_str(" ");
                print_u32_hex(lower_target);
                print_str(" ");
                print_u32_hex(timeout);
                print_str(" ");
                print_u64_hex(t64_state.base);
                print_str(" ");
                print_u64_hex(t64_state.target);
                print_str("\n");
            }
        }
        t64_state.needs_update = 0;
        break; /* timer was set OK */
    }
}

/**
 * @brief   hardware timer interrupt handler
 *
 * @param[in]   arg     Argument from periph/timer (ignored)
 * @param[in]   chan    Timer channel (ignored)
 */
static void t64_cb(void *arg, int chan)
{
    (void)arg;
    (void)chan;
    unsigned int now = timer_read(T64_DEV);
    t64_state.needs_update = 1;

    if (!t64_state.awaiting_overflow) {
        /* Target was hit */
        t64_state.target = 0;
        t64_update_timeouts(now);
        if (t64_state.cb) {
            t64_state.cb(t64_state.arg);
        }
    }
    else {
        t64_update_timeouts(now);
    }
}

int t64_init(unsigned long freq, t64_cb_t cb, void *arg)
{
    unsigned mask = irq_disable();
    t64_state.cb = cb;
    t64_state.arg = arg;
    t64_state.base = 0;
    t64_state.target = 0;
    t64_state.lower_partition = 0;
    t64_state.needs_update = 1;

    int res = timer_init(T64_DEV, freq, t64_cb, &t64_state);
    if (res < 0) {
        irq_restore(mask);
        return res;
    }
    t64_update_timeouts(0);
    irq_restore(mask);
    return 0;
}

void t64_stop(void)
{
    timer_stop(T64_DEV);
}

void t64_start(void)
{
    timer_start(T64_DEV);
}

uint64_t t64_now(void)
{
    unsigned mask = irq_disable();
    unsigned int now = timer_read(T64_DEV);
    t64_update_partition(now);
    uint64_t ret = (t64_state.base + (now & (T64_PARTITION_MASK)));
    irq_restore(mask);
    return ret;
}

void t64_set(uint32_t timeout)
{
    unsigned mask = irq_disable();
    unsigned int now = timer_read(T64_DEV);
    t64_state.target = (t64_state.base + (now & (T64_PARTITION_MASK))) + timeout;
    t64_state.needs_update = 1;
    /* Reuse the now value to avoid redundant timer_read calls */
    t64_update_timeouts(now);
    irq_restore(mask);
}

void t64_set_absolute(uint64_t target)
{
    unsigned mask = irq_disable();
    t64_state.target = target;
    t64_state.needs_update = 1;
    t64_update_timeouts(timer_read(T64_DEV));
    irq_restore(mask);
}
