/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "transport.h"
#include "mcu_transport.h"
#include <hidapi/hidapi.h>

static int hidapi_read(void *hndl, unsigned char id, void *buf, unsigned size)
{
	return hid_read(hndl, buf, size);
}

static int hidapi_write(void *hndl, unsigned char id, const void *buf, unsigned size)
{
	return hid_write(hndl, buf, size);
}

static void hidapi_close(void *hndl)
{
	hid_close(hndl);
	hid_exit();
}

struct transport *hidapi_transport_open(uint16_t vid, uint16_t pid)
{
	int rc;
	hid_device *dev;

	rc = hid_init();

	dev = hid_open(vid, pid, NULL);
	if (dev == NULL) {
		fprintf(stderr, "hid open %04x:%04x: %s\n", vid, pid, strerror(errno));
		hid_exit();
		return NULL;
	}

	return mcu_transport_open(dev, hidapi_close, hidapi_read, hidapi_write);
}
