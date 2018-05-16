/*
 * Copyright (C) 2018 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @ingroup     drivers_tacho
 * @{
 *
 * @file
 * @brief       GPIO tachometer driver implementation
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include <stdint.h>
#include <string.h>
#include "tacho.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static void tacho_rotate_buffers(tacho_t *dev)
{
    unsigned next = (dev->idx + 1) % dev->num_bufs;
    tacho_interval_t *ival = &dev->bufs[next];
    ival->count = 0;
    ival->time_start = ival->time_end = dev->bufs[dev->idx].time_end;
    dev->idx = next;
}

/* Accumulate pulse count */
static void tacho_trigger(void *arg)
{
    tacho_t *dev = (tacho_t *)arg;
    tacho_interval_t *ival = &dev->bufs[dev->idx];
    ++ival->count;
    ival->time_end = xtimer_now();
    if (xtimer_less(dev->min_duration, xtimer_diff(ival->time_end, ival->time_start))) {
        /* Rotate buffers when enough time has passed */
        tacho_rotate_buffers(dev);
    }
}

int tacho_init(tacho_t *dev, const tacho_params_t *params)
{
    int res = gpio_init_int(params->gpio, params->gpio_mode, params->gpio_flank, tacho_trigger, dev);
    if (res != 0) {
        return res;
    }

    dev->idx = 0;
    assert(dev->bufs);
    assert(dev->num_bufs);
    memset(dev->bufs, 0, dev->num_bufs * sizeof(dev->bufs[0]));
    return 0;
}

void tacho_read(const tacho_t *dev, unsigned *count, uint32_t *duration)
{
    unsigned idx = dev->idx;
    xtimer_ticks32_t now = xtimer_now();
    if ((*duration) < xtimer_usec_from_ticks(xtimer_diff(now, dev->bufs[idx].time_end))) {
        /* no pulses detected within the duration */
        *duration = 0;
        *count = 0;
        return;
    }
    unsigned n = dev->num_bufs;
    unsigned sum_count = 0;
    uint32_t sum_duration = 0;
    while ((n > 0) && (sum_duration < (*duration))) {
        tacho_interval_t *ival = &dev->bufs[idx];
        sum_count += ival->count;
        sum_duration += xtimer_usec_from_ticks(xtimer_diff(ival->time_end, ival->time_start));
        --n;
        idx = (dev->num_bufs + idx - 1) % dev->num_bufs;
    }
    *count = sum_count;
    *duration = sum_duration;
}
