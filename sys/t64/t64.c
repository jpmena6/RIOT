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

#define ENABLE_DEBUG 1
#include "debug.h"
#include "fmt.h"

#ifdef T64_LOWER_TYPE
typedef T64_LOWER_TYPE t64_lower_t;
#else
typedef unsigned int t64_lower_t;
#endif
#ifndef T64_DEV
#define T64_DEV (TIMER_DEV(0))
#endif
#ifndef T64_CHAN
#define T64_CHAN (0)
#endif

#ifndef T64_LOWER_MAX
#define T64_LOWER_MAX (0xfffffffful)
#endif
/* Partition size, must be a power of two and less than T64_LOWER_MAX */
#define T64_PARTITION (((T64_LOWER_MAX) >> 20) + 1)
/* in-partition volatile bits */
#define T64_PARTITION_MASK ((T64_PARTITION - 1))
/* Minimum relative timeout, used when the target has been missed */
#define T64_MIN_REL 1

#ifndef T64_TRACE
#define T64_TRACE   0
#endif

/* Target value for unset timers */
#define T64_TARGET_UNSET (~0ull) /* == at overflow, a few millenia from now */

typedef struct {
    /**
     * @brief   Timer device to use
     */
    tim_t dev;
    /**
     * @brief   Timer channel to use
     *
     * Use 0 if unsure
     */
    int channel;
    /**
     * @brief   Partition size, must be a power of two
     */
    t64_lower_t partition_size;
    /**
     * @brief   Bit mask for the counter bits inside the partition
     *
     * set this to `partition_size - 1`
     */
    t64_lower_t partition_mask;
    /**
     * @brief   Maximum settable timeout for the lower level timer
     */
    t64_lower_t lower_max;
} t64_params_t;

typedef struct {
    uint64_t base;
    uint64_t target;
    t64_cb_t cb;
    void *arg;
    t64_lower_t partition;
    unsigned int needs_update;
    unsigned int started;
} t64_state_t;

#ifndef T64_NUMOF
#define T64_NUMOF 1
#endif
#ifndef T64_PARAMS
#define T64_PARAMS (const t64_params_t[]){ \
    { \
    .dev            = TIMER_DEV(0), \
    .channel        = 0, \
    .partition_size = 0x4000ul, \
    .partition_mask = 0x4000ul - 1, \
    .lower_max      = 0xfffffffful, \
    }, \
}
#endif

static const t64_params_t t64_params[T64_NUMOF] = T64_PARAMS;

static t64_state_t t64_state[T64_NUMOF];

/**
 * @brief   Check for partition transitions and update base accordingly
 *
 * @param[in]   idx     T64 timer index
 * @param[in]   now     'now' time (from @ref timer_read())
 */
static void t64_checkpoint(unsigned int idx, t64_lower_t now)
{
    assert(idx < T64_NUMOF);
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];
    t64_lower_t partition = now & ~(params->partition_mask);
    if (partition != state->partition) {
        if (T64_TRACE) {
            print_str("next ");
            print_u32_hex(now);
            print_str(" ");
            print_u32_hex(partition);
            print_str(" ");
            print_u32_hex(state->partition);
            print_str(" ");
            print_u64_hex(state->base);
            print("\n", 1);
        }
        state->base += (partition - state->partition) & (params->lower_max);
        state->partition = partition;
        assert(state->partition == (((t64_lower_t)state->base) & (params->lower_max)));
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
static void t64_update_timeouts(unsigned int idx, t64_lower_t before)
{
    assert(idx < T64_NUMOF);
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];
    /* Keep trying until we manage to set a timer */
    while(1) {
        /* Keep the base offset up to date */
        t64_checkpoint(idx, before);
        if (!state->needs_update) {
            /* Early exit to avoid unnecessary 64 bit target time computations */
            break;
        }
        uint64_t now64 = state->base + (before & params->partition_mask);
        if (state->target <= now64) {
            if (T64_TRACE) {
                print_str("<<<z ");
                print_u32_hex(before);
                print_str(" ");
                print_u64_hex(state->target);
                print_str(" ");
                print_u64_hex(state->base + (before & params->partition_mask));
                print_str(" ");
                print_u64_hex(state->base);
                print_str("\n");
            }
            state->target = T64_TARGET_UNSET;
            state->needs_update = 1;
            if (state->cb) {
                state->cb(state->arg);
            }
            before = timer_read(params->dev);
            continue;
        }
        t64_lower_t lower_target;
        if ((state->target - now64) >= params->partition_size) {
            /* The real target is more than one partition duration away */
            lower_target = (before + params->partition_size) & params->lower_max;
            if (T64_TRACE) {
                print_str("part ");
            }
        }
        else {
            /* Set real target */
            /* discard top bits and compute lower timer target phase */
            lower_target = (t64_lower_t)state->target & params->lower_max;
            if (T64_TRACE) {
                print_str("real ");
            }
        }
        timer_set_absolute(params->dev, params->channel, lower_target);
        t64_lower_t after = 0;
        if (state->started) {
            /* There is a danger of setting an absolute timer target in the low
             * level timer since we might run past the target before the timer
             * has been updated with the new target time */
            after = timer_read(params->dev);
        }
        if (T64_TRACE) {
            print_u32_hex(before);
            print_str(" ");
            print_u32_hex(after);
            print_str(" ");
            print_u32_hex(lower_target);
            print_str(" ");
            print_u32_hex(state->partition);
            print_str(" ");
            print_u64_hex(state->base);
            print_str(" ");
            print_u64_hex(state->target);
            print_str("\n");
        }
        if (state->started) {
            if ((lower_target - before) <= (after - before)) {
                /* We passed the target while setting the timeout, abort and retry */
                timer_clear(params->dev, params->channel);
                before = after;
                /* try again */
                state->needs_update = 1;
                if (T64_TRACE) {
                    print_str("retry\n");
                }
                continue;
            }
        }
        /* timer was set OK */
        state->needs_update = 0;
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
    (void)chan;
    unsigned int idx = (unsigned int)arg;
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];
    if (T64_TRACE) {
        print_str("t64cb\n");
    }
    unsigned int now = timer_read(params->dev);
    state->needs_update = 1;
    t64_update_timeouts(idx, now);
}

int t64_init(unsigned int idx, unsigned long freq, t64_cb_t cb, void *arg)
{
    assert(idx < T64_NUMOF);
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];
    unsigned mask = irq_disable();
    state->cb = cb;
    state->arg = arg;
    state->base = 0;
    state->target = T64_TARGET_UNSET;
    state->partition = 0;
    state->needs_update = 1;
    state->started = 1;

    int res = timer_init(params->dev, freq, t64_cb, (void *)idx);
    if (res < 0) {
        irq_restore(mask);
        return res;
    }
    t64_update_timeouts(idx, timer_read(params->dev));
    irq_restore(mask);
    return 0;
}

void t64_stop(unsigned int idx)
{
    assert(idx < T64_NUMOF);
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];

    unsigned mask = irq_disable();
    state->started = 0;
    timer_stop(params->dev);
    irq_restore(mask);
}

void t64_start(unsigned int idx)
{
    assert(idx < T64_NUMOF);
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];

    unsigned mask = irq_disable();
    state->started = 1;
    timer_start(params->dev);
    irq_restore(mask);
}

uint64_t t64_now(unsigned int idx)
{
    assert(idx < T64_NUMOF);
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];

    unsigned mask = irq_disable();
    unsigned int now = timer_read(params->dev);
    t64_checkpoint(idx, now);
    uint64_t ret = (state->base + (now & params->partition_mask));
    irq_restore(mask);
    return ret;
}

void t64_set(unsigned int idx, uint32_t timeout)
{
    assert(idx < T64_NUMOF);
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];

    unsigned mask = irq_disable();
    unsigned int now = timer_read(params->dev);
    t64_checkpoint(idx, now);
    state->target = (state->base + (now & params->partition_mask)) + timeout;
    state->needs_update = 1;
    /* Reuse the now value to avoid redundant timer_read calls */
    t64_update_timeouts(idx, now);
    irq_restore(mask);
}

void t64_set_absolute(unsigned int idx, uint64_t target)
{
    assert(idx < T64_NUMOF);
    const t64_params_t *params = &t64_params[idx];
    t64_state_t *state = &t64_state[idx];

    unsigned mask = irq_disable();
    state->target = target;
    state->needs_update = 1;
    t64_update_timeouts(idx, timer_read(params->dev));
    irq_restore(mask);
}
