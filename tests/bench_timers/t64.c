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

#include <stdint.h>
#include "t64.h"
#include "irq.h"
#include "periph/timer.h"

#ifndef T64_DEV
#define T64_DEV (TIMER_DEV(0))
#endif
#ifndef T64_CHAN
#define T64_CHAN (0)
#endif
#ifndef T64_LOWER_PERIOD
#define T64_LOWER_PERIOD ((uint64_t)0x100000000ull)
#endif
#ifndef T64_LOWER_MAX
#define T64_LOWER_MAX (T64_LOWER_PERIOD - 1)
#endif

typedef struct {
    uint64_t base;
    uint64_t target;
    t64_cb_t cb;
    void *arg;
    unsigned awaiting_overflow;
} t64_state_t;

t64_state_t t64_state;

/**
 * @brief   Set next low level timer timeout
 *
 * This will set the real target timer if it is within the same period as the
 * current time, or set an overflow timeout otherwise.
 *
 * @pre IRQ disabled
 */
void t64_set_or_split(void)
{
    /* Keep trying until we manage to set a timer */
    while(1) {
        if ((t64_state.target == 0) ||
            ((t64_state.base ^ t64_state.target) > T64_LOWER_PERIOD)) {
            /* Timer overflow will occur before the target time, or no target set */
            t64_state.awaiting_overflow = 1;
            unsigned int before = timer_read((T64_DEV));
            timer_set_absolute((T64_DEV), (T64_CHAN), (T64_LOWER_MAX));
            unsigned int after = timer_read((T64_DEV));
            if (after < before) {
                /* Overflow occurred while setting the timeout, abort and retry */
                timer_clear(T64_DEV, T64_CHAN);
                t64_state.base += (T64_LOWER_PERIOD);
                continue;
            }
        }
        else {
            /* Set real target */
            t64_state.awaiting_overflow = 0;
            /* discard top bits and compute lower timer target phase */
            unsigned int lower_target = ((unsigned int)t64_state.target) - ((unsigned int)t64_state.base);
            unsigned int before = timer_read((T64_DEV));
            unsigned int timeout = lower_target - before;
            timer_set((T64_DEV), (T64_CHAN), timeout);
            unsigned int after = timer_read((T64_DEV));
            if (after < before) {
                /* Overflow occurred while setting the timeout, increment base */
                t64_state.base += (T64_LOWER_PERIOD);
            }
        }
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

    if (t64_state.awaiting_overflow) {
        t64_state.base += (T64_LOWER_PERIOD);
        t64_set_or_split();
    }
    else {
        /* Target was hit */
        t64_state.target = 0;
        t64_set_or_split();
        if (t64_state.cb) {
            t64_state.cb(t64_state.arg);
        }
    }
}

int t64_init(unsigned long freq, t64_cb_t cb, void *arg)
{
    unsigned mask = irq_disable();
    t64_state.cb = cb;
    t64_state.arg = arg;
    t64_state.base = 0;
    t64_state.target = 0;

    int res = timer_init((T64_DEV), freq, t64_cb, &t64_state);
    if (res < 0) {
        irq_restore(mask);
        return res;
    }
    t64_set_or_split();
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
    return (t64_state.base + timer_read(T64_DEV));
}

void t64_set(uint32_t timeout)
{
    unsigned mask = irq_disable();
    t64_state.target = t64_now() + timeout;
    t64_set_or_split();
    irq_restore(mask);
}

void t64_set_absolute(uint64_t target)
{
    unsigned mask = irq_disable();
    t64_state.target = target;
    t64_set_or_split();
    irq_restore(mask);
}
