/*
 * Copyright (C) 2018 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_tacho
 * @{
 *
 * @file
 * @brief       Tacho driver SAUL mapping
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include "saul.h"
#include "tacho.h"

#if 0
static int read_tacho(const void *dev, phydat_t *res)
{
    res->val[0] = tacho_read(dev);
    res->unit  = UNIT_;
    res->scale = 0;
    return 1;
}

const saul_driver_t tacho_saul_driver = {
    .read  = read_tacho,
    .write = saul_notsup,
    .type  = SAUL_SENSE_COUNT,
};
#endif
