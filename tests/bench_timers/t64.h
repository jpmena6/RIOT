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
 * @brief     Extends a 16 bit or 32 bit timer to a 64 bit virtual timer.
 *
 *
 * @{
 * @file
 * @brief   t64 interface definitions
 * @author  Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */
#ifndef T64_H
#define T64_H

#include <stdint.h>

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
 * @brief   Initialize the t64 library and the underlying hardware timer
 *
 * The timer will be started automatically after initialization.
 *
 * @param[in] freq          requested number of ticks per second
 * @param[in] cb            this callback is called in interrupt context, the
 *                          emitting channel is passed as argument
 * @param[in] arg           argument to the callback
 *
 * @return                  0 on success
 * @return                  <0 on error
 */
int t64_init(unsigned long freq, t64_cb_t cb, void *arg);

/**
 * @brief   Stop the timer
 */
void t64_stop(void);

/**
 * @brief   Start the timer
 *
 * @note this is only necessary if the timer was stopped before, the timer is
 *       always running after initialization.
 */
void t64_start(void);

/**
 * @brief   Get the current count on the 64 bit virtual timer
 *
 * @return  the current counter value
 */
uint64_t t64_now(void);

/**
 * @brief   Set a timer target relative to the current time
 *
 * @param[in]   timeout relative timeout
 */
void t64_set(uint32_t timeout);

/**
 * @brief   Set an absolute timer target
 *
 * @param[in]   target  absolute target
 */
void t64_set_absolute(uint64_t target);

/**
 * @brief   Clear the current timeout
 *
 * The timer will be kept running if already running, but the current timeout
 * will be cleared.
 */
static inline void t64_clear(void)
{
    t64_set_absolute(0);
}

#ifdef __cplusplus
}
#endif

/** @} */
#endif /* T64_H */
