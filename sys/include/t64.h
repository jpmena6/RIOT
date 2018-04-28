/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup  sys_t64 T64 timer extender
 * @ingroup   sys
 * @brief     Extends a hardware timer to a 64 bit virtual timer.
 *
 * Provides a monotonic 64 bit timer by wrapping a hardware timer, which can be
 * of any width, using checkpointed interval partitioning.
 *
 * ### Theory of operation
 *
 * The hardware timer range is split into multiple equal length intervals called
 * partitions. The hardware timer target is never scheduled further into the
 * future than the length of one partition, this prevents the ambiguity in how
 * the software should interpret read timer values in relation to setting timer
 * targets.
 *
 * An internal state struct is used to keep track of the 64 bit timer target,
 * some internal flags, and the 64 bit offset from the hardware timer.
 *
 * #### Checkpointing
 *
 * A checkpoint is updated every time the hardware timer is read by the library.
 * The 64 bit timer offset is updated whenever the hardware timer transitions
 * into a new partition.
 *
 * #### Long timeouts
 *
 * When a timer target is requested which is further than one partition duration
 * in the future, the t64 wrapper will set successive partition length timeouts
 * on the hardware timer until the target is within one partition from the
 * current time.
 *
 * #### Past targets
 *
 * When a timer target is requested to a time in the past, the callback will be
 * immediately called, without setting a hardware timer.
 *
 * #### Race conditions
 *
 * An extra check is made after setting a hardware timer to ensure that the
 * current time did not pass the timer target while setting the hardware timer.
 * When this occurs, it is impossible for the library to know whether the
 * hardware timer did catch the target or if the time had already passed the
 * target when the hardware timer target was updated.
 * If the library detects that the target was passed while setting the target,
 * the timer target will be cleared and the callback function will be called
 * directly from the T64 library instead of from the timer ISR.
 *
 * @{
 * @file
 * @brief   t64 interface definitions
 * @author  Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */
#ifndef T64_H
#define T64_H

#include <stdint.h>
#include "periph/timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Signature of timeout callback function
 *
 * @param[in] arg       optional context for the callback
 */
typedef void (*t64_cb_t)(void *arg);

/**
 * @brief   Counter data type for the underlying timer
 */
#ifdef T64_LOWER_TYPE
typedef T64_LOWER_TYPE t64_lower_t;
#else
typedef unsigned int t64_lower_t;
#endif

/**
 * @brief   T64 configuration parameters
 */
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
     * @brief   Maximum settable timeout for the lower level timer
     */
    t64_lower_t lower_max;
    /**
     * @brief   Partition size, must be a power of two
     *
     * Suggested value: `(lower_max >> 2) + 1`
     */
    t64_lower_t partition_size;
    /**
     * @brief   Bit mask for the counter bits inside the partition
     *
     * Set this to `partition_size - 1`
     */
    t64_lower_t partition_mask;
} t64_params_t;

/**
 * @brief   Initialize the t64 library and the underlying hardware timer
 *
 * The timer will be started automatically after initialization.
 *
 * @param[in]   idx     t64 instance number
 * @param[in]   freq    requested number of ticks per second
 * @param[in]   cb      the callback that will be called every time the timer expires
 * @param[in]   arg     argument to the callback
 *
 * @return              0 on success
 * @return              <0 on error
 */
int t64_init(unsigned int idx, unsigned long freq, t64_cb_t cb, void *arg);

/**
 * @brief   Stop the timer
 *
 * @param[in]   idx     t64 instance number
 */
void t64_stop(unsigned int idx);

/**
 * @brief   Start the timer
 *
 * @note this is only necessary if the timer was stopped before, the timer is
 *       always running after initialization.
 *
 * @param[in]   idx     t64 instance number
 */
void t64_start(unsigned int idx);

/**
 * @brief   Get the current count on the 64 bit virtual timer
 *
 * @param[in]   idx     t64 instance number
 *
 * @return  the current counter value
 */
uint64_t t64_now(unsigned int idx);

/**
 * @brief   Set a timer target relative to the current time
 *
 * @param[in]   idx     t64 instance number
 * @param[in]   timeout relative timeout
 */
void t64_set(unsigned int idx, uint32_t timeout);

/**
 * @brief   Set an absolute timer target
 *
 * @param[in]   idx     t64 instance number
 * @param[in]   target  absolute target
 */
void t64_set_absolute(unsigned int idx, uint64_t target);

/**
 * @brief   Clear the current timeout
 *
 * The timer will be kept running if already running, but the current timeout
 * will be cleared.
 *
 * @param[in]   idx     t64 instance number
 */
static inline void t64_clear(unsigned int idx)
{
    t64_set_absolute(idx, 0);
}

#ifdef __cplusplus
}
#endif

/** @} */
#endif /* T64_H */
