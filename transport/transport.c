/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "transport.h"

struct transport *serial_transport_open(const char *dev, unsigned speed);
struct transport *usb_transport_open(uint16_t vid, uint16_t pid, int iface, unsigned flags);
struct transport *hidapi_transport_open(uint16_t vid, uint16_t pid);

struct transport *transport_open(const char *transport_name, union transport_param *param)
{
	if (!strcmp(transport_name, TRANSPORT_IFACE_HIDAPI)) {
		return hidapi_transport_open(param->hidapi.vid, param->hidapi.pid);
	}

	if (!strcmp(transport_name, TRANSPORT_IFACE_LIBUSB)) {
		return usb_transport_open(param->libusb.vid, param->libusb.pid,
			param->libusb.iface, param->libusb.flags);
	}

	if (!strcmp(transport_name, TRANSPORT_IFACE_SERAIL)) {
		return serial_transport_open(param->serial.tty, param->serial.speed);
	}

	return NULL;
}
