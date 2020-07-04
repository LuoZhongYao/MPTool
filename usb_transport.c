#include "defs.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "transport.h"
#include <libusb-1.0/libusb.h>

#define USB_START_TIMEOUT		2000
#define USB_WRITE_TIMEOUT		2000
#define USB_READ_TIMEOUT		2000
#define USB_TRANS_TIMEOUT		2000
#define FLAG_AUTO_DETACH_KERNEL_DRIVER	0x0001

#define USB_TRANS_CMD_START			0x01
#define USB_TRANS_CMD_SET_BAUDRATE	0x02
#define USB_TRANS_CMD_WRITE			0x03
#define USB_TRANS_CMD_READ			0x04
#define USB_TRANS_CMD_FINISH		0x05

#define TRANS_BLOCK_SIZE	60

struct usb_transport {
	int flags;
	int iface;
	libusb_device_handle *hndl;
	struct transport transport;
};

static int hotplug_arrived_callback(struct libusb_context *ctx, struct libusb_device *dev,
	libusb_hotplug_event event, void *user_data)
{
	int rc;
	libusb_device_handle **hndl = user_data;
	struct libusb_device_descriptor desc;

	(void)libusb_get_device_descriptor(dev, &desc);

	if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
		if (*hndl != NULL)
			return 0;

		rc = libusb_open(dev, hndl);
		if (LIBUSB_SUCCESS != rc) {
			printf("libusb_open: %s\n", libusb_strerror(rc));
			exit(1);
		}
	}

	return 0;
}

static int hotplug_left_callback(struct libusb_context *ctx, struct libusb_device *dev,
	libusb_hotplug_event ev, void *user_data)
{
	int rc;
	if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == ev) {
	}

	return 0;
}

static unsigned char checksum(unsigned char *data, unsigned size)
{
	int i;
	unsigned char sum = 0;

	for (i = 0; i < size; i++) {
		sum += data[i];
	}

	return sum;
}

static int usb_init(int usb_log_level)
{
	libusb_init(NULL);

	if (usb_log_level >= LIBUSB_LOG_LEVEL_NONE && usb_log_level <= LIBUSB_LOG_LEVEL_DEBUG) {
		libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, usb_log_level);
	}

	return 0;
}

static libusb_device_handle *usb_open_timeout(uint16_t vid, uint16_t pid,
	int iface, uint32_t ms, int *res, int flags)
{
	struct timeval tv;
	libusb_device_handle *hndl;
	libusb_hotplug_callback_handle cb;

	*res = 0;
	hndl = libusb_open_device_with_vid_pid(NULL, vid, pid);
	if (hndl == NULL) {
		tv.tv_sec = ms / 1000;
		tv.tv_usec = (ms % 1000) * 1000;

		*res  = libusb_hotplug_register_callback(NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
			0, vid, pid, LIBUSB_HOTPLUG_MATCH_ANY, hotplug_arrived_callback, &hndl, &cb);
		if (*res != LIBUSB_SUCCESS) {
			return NULL;
		}

		*res = libusb_handle_events_timeout(NULL, &tv);
		libusb_hotplug_deregister_callback(NULL, cb);
	}

	if (hndl != NULL) {
		if (flags & FLAG_AUTO_DETACH_KERNEL_DRIVER) {
			libusb_set_auto_detach_kernel_driver(hndl, 1);
		}

		*res = libusb_claim_interface(hndl, iface);
		if (*res != LIBUSB_SUCCESS) {
			libusb_close(hndl);
			return NULL;
		}
	}

	return hndl;
}

static int usb_write_command(libusb_device_handle *hndl, uint8_t cmd, const void *param, uint8_t size, unsigned timeout)
{
	int rc;
	int trans_number;
	int retry = 3;
	uint8_t tmp[64];
	uint8_t rsp[64];

	memset(tmp, 0, 64);

	tmp[0] = 0x02;
	tmp[1] = cmd;
	tmp[2] = size;
	memcpy(tmp + 3, param, size);
	tmp[63] = checksum(tmp, 63);

	rc = libusb_interrupt_transfer(hndl, 2, tmp, 64, &trans_number, USB_TRANS_TIMEOUT);

	if (rc) {
		return rc;
	}

	while (retry--) {
		rc = libusb_interrupt_transfer(hndl, 0x81, rsp, 64, &trans_number, timeout);
		if (rc != 0) {
			return rc;
		}

		if (trans_number != 64 || rsp[0] != 0x01) {
			return LIBUSB_ERROR_OTHER;
		}

		if (rsp[1] == 0 && rsp[2] == 2 && rsp[3] == cmd && rsp[4] == 0) {
			return 0;
		}
	}

	return LIBUSB_ERROR_TIMEOUT;
}

static int usb_read_block(libusb_device_handle *hndl, void *buf, unsigned size, unsigned *read_size, unsigned timeout)
{
	int rc;
	int sum;
	int trans_number;
	uint8_t crc;
	uint8_t tmp[64];
	uint8_t rsp[64];

	memset(tmp, 0, 64);
	tmp[0] = 0x02;
	tmp[1] = USB_TRANS_CMD_READ;
	tmp[2] = 1;
	tmp[3] = size;
	tmp[63] = checksum(tmp, 63);

	*read_size = 0;
	rc = libusb_interrupt_transfer(hndl, 2, tmp, 64, &trans_number, USB_TRANS_TIMEOUT);
	if (rc != 0) {
		return rc;
	}

	rc = libusb_interrupt_transfer(hndl, 0x81, rsp, 64, &trans_number, timeout);
	if (rc != 0) {
		return rc;
	}

	if (trans_number != 64 || rsp[0] != 0x01) {
		return LIBUSB_ERROR_OTHER;
	}

	//sum = checksum(rsp, 63);
	//if (sum != rsp[63]) {
	//	return LIBUSB_ERROR_OTHER + 0x01;
	//}

	if (rsp[1] == 0x00 && rsp[2] == 2 && rsp[3] == 0x04) {
		return LIBUSB_ERROR_OTHER + rsp[4];
	}

	if (rsp[1] != 0x04) {
		return LIBUSB_ERROR_OTHER;
	}

	if (rsp[2] > size) {
		return LIBUSB_ERROR_OTHER;
	}

	memcpy(buf, rsp + 3, rsp[2]);
	*read_size = rsp[2];

	return 0;
}

static int usb_write(struct transport *trans, const void *buf, unsigned size)
{
	int rc;
	unsigned write_number = 0;
	struct usb_transport *usb = container_of(trans, struct usb_transport, transport);

	while (write_number < size) {
		unsigned count = MIN(size - write_number, TRANS_BLOCK_SIZE);

		rc = usb_write_command(usb->hndl, USB_TRANS_CMD_WRITE,
			buf + write_number, count, USB_WRITE_TIMEOUT);
		if (rc != 0) {
			return write_number;
		}
		write_number += count;
	}

	return write_number;
}

static int usb_read(struct transport *trans, void *buf, unsigned size)
{
	int rc;
	unsigned read_number = 0;
	struct usb_transport *usb = container_of(trans, struct usb_transport, transport);

	while (read_number < size) {
		unsigned bytes = 0;
		unsigned count = MIN(size - read_number, TRANS_BLOCK_SIZE);

		rc = usb_read_block(usb->hndl, buf + read_number, count, &bytes, USB_READ_TIMEOUT);
		if (rc != 0) {
			return read_number;
		}
		read_number += bytes;
	}

	return read_number;
}

static void usb_close(struct transport *trans)
{
	struct usb_transport *usb = container_of(trans, struct usb_transport, transport);

	usb_write_command(usb->hndl, USB_TRANS_CMD_FINISH,
		NULL, 0, USB_START_TIMEOUT);

	if (usb->flags & FLAG_AUTO_DETACH_KERNEL_DRIVER) {
		libusb_release_interface(usb->hndl, usb->iface);
	}
	libusb_close(usb->hndl);
	libusb_exit(NULL);

	free(usb);
}

static int usb_set_baudrate(struct transport *trans, unsigned speed)
{
	uint32_t baudrate = speed;
	struct usb_transport *usb = container_of(trans, struct usb_transport, transport);

	return usb_write_command(usb->hndl, USB_TRANS_CMD_SET_BAUDRATE,
		&baudrate, 4, USB_WRITE_TIMEOUT);
}

static const struct transport_ops usb_transport_ops = {
	.write = usb_write,
	.read = usb_read,
	.close = usb_close,
	.set_baudrate = usb_set_baudrate,
};

struct transport *usb_transport_open(uint16_t vid, uint16_t pid, int iface, unsigned flags)
{
	int rc;
	uint32_t baudrate = 115200;
	libusb_device_handle *hndl;
	struct usb_transport *usb;

	usb_init(LIBUSB_LOG_LEVEL_NONE);
	hndl = usb_open_timeout(vid, pid, iface, 10 * 1000, &rc, flags);
	if (hndl == NULL) {
		libusb_exit(NULL);
		return NULL;
	}

	usb = malloc(sizeof(struct usb_transport));
	usb->hndl = hndl;
	usb->flags = flags;
	usb->iface = iface;
	usb->transport.ops = &usb_transport_ops;

	rc = usb_write_command(hndl, USB_TRANS_CMD_START, &baudrate, 4, USB_START_TIMEOUT);
	if (rc != 0) {
		fprintf(stderr, "start MP failure: %s\n", libusb_strerror(rc));
		usb_close(&usb->transport);
		return NULL;
	}

	return &usb->transport;
}
