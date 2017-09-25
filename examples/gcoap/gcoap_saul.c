/*
 * Copyright (c) 2015-2016 Ken Bannister. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       gcoap CLI support
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net/gcoap.h"
#include "od.h"
#include "fmt.h"
#include "saul_reg.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static ssize_t _res_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len);

/* CoAP resources */
static const coap_resource_t _resources[] = {
    { "/saul", COAP_GET, _res_handler },
};
static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

/*
 * Server callback for the resource.
 */
static ssize_t _res_handler(coap_pkt_t* pdu, uint8_t *buf, size_t len)
{
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    saul_reg_t *dev = saul_reg_find_nth(7);
    phydat_t res;

    int dim = saul_reg_read(dev, &res);
    (void)dim;

    size_t payload_len = fmt_u16_dec((char *)pdu->payload, res.val[0]);

    return gcoap_finish(pdu, payload_len, COAP_FORMAT_TEXT);
}

void gcoap_saul_init(void)
{
    saul_reg_t *dev;
    phydat_t dat = { .val = {1, 0, 0}};
    dev = saul_reg_find_nth(0);
    saul_reg_write(dev, &dat);
    dev = saul_reg_find_nth(1);
    saul_reg_write(dev, &dat);
    dev = saul_reg_find_nth(2);
    saul_reg_write(dev, &dat);
    gcoap_register_listener(&_listener);
}
