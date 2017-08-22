/*
 * Copyright (C) 2017 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_contikimac ContikiMAC compatible MAC layer
 * @ingroup     net_gnrc
 * @brief       Duty cycling MAC protocol for low power communication over IEEE 802.15.4 networks
 *
 * @see http://dunkels.com/adam/dunkels11contikimac.pdf
 * @see https://arxiv.org/abs/1404.3589
 *
 * @todo Write proper description
 *
 * @{
 *
 * @file
 * @brief       Interface definition for the ContikiMAC layer
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef NET_GNRC_CONTIKIMAC_CONTIKIMAC_H
#define NET_GNRC_CONTIKIMAC_CONTIKIMAC_H

#include "kernel_types.h"
#include "net/gnrc/netdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a network interface with ContikiMAC
 *
 * The initialization starts a new thread that connects to the given netdev
 * device and starts a link layer event loop.
 *
 * @param[in] stack         stack for the control thread
 * @param[in] stacksize     size of *stack*
 * @param[in] priority      priority for the thread
 * @param[in] name          name of the thread
 * @param[in] dev           netdev device, needs to be already initialized
 *
 * @return                  PID of thread on success
 * @return                  -EINVAL if creation of thread fails
 * @return                  -ENODEV if *dev* is invalid
 */
kernel_pid_t gnrc_contikimac_init(char *stack, int stacksize, char priority,
                             const char *name, gnrc_netdev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LWMAC_LWMAC_H */
/** @} */
