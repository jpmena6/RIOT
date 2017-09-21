/*
 * Copyright (C) 2017 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net
 * @file
 * @brief       GNRC ContikiMAC timing settings for O-QPSK 250 kbit/s
 *
 * These timings are valid for STD IEEE 802.15.4 channel page 0 in the 2.4 GHz
 * band, and channel page 1 in the 915 MHz band.
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @}
 */

#include "net/gnrc/contikimac/contikimac.h"

const contikimac_params_t contikimac_params_OQPSK250 = {
    .channel_check_period = 1000000ul / 8, /* T_w, 8 Hz */
    .cca_cycle_period = 54 * 16 / 2, /* T_c = T_i / (n_c - 1) */
    .inter_packet_interval = 54 * 16, /* T_i = Ack timeout */
    .after_ed_scan_timeout = 5000, /* > T_l */
    .after_ed_scan_interval = 500, /* < T_i */
    .listen_timeout = 54 * 16 + 1000, /* > T_i */
    .rx_timeout = 4500, /* > T_l */
    .cca_count_max = 3, /* = n_c */
};
