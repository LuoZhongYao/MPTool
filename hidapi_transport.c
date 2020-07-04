/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "transport.h"
#include "rtlmptool.h"
#include "mcu_transport.h"
#include "hidapi_transport.h"
#include <hidapi/hidapi.h>

static int hidapi_read(void *hndl, unsigned char id, void *buf, unsigned size)
{
	return hid_read(hndl, buf, size);
}

static int hidapi_write(void *hndl, unsigned char id, const void *buf, unsigned size)
{
	return hid_write(hndl, buf, size);
}

struct transport *hidapi_transport_open(uint16_t vid, uint16_t pid)
{
	int rc;
	hid_device *dev;

	rc = hid_init();

	dev = hid_open(vid, pid, NULL);
	if (dev == NULL) {
		fprintf(stderr, "hid open %04x:%04x: %s\n", vid, pid, strerror(errno));
		return NULL;
	}

	return mcu_transport_open(dev, hidapi_read, hidapi_write);
}
