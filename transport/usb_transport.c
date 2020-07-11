#include "defs.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mcu_transport.h"
#include <libusb-1.0/libusb.h>

#define USB_TRANS_TIMEOUT	2000
#define FLAG_AUTO_DETACH_KERNEL_DRIVER	0x0001

struct usb_context {
	int flags, iface;
	libusb_device_handle *hndl;
};

static int hotplug_arrived_callback(libusb_context *ctx, libusb_device *dev,
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

static int usb_init(int usb_log_level)
{
	int rc;
	libusb_init(NULL);

#if defined(__WIN32__)
	rc = libusb_set_option(NULL, LIBUSB_OPTION_USE_USBDK);
	if (rc != LIBUSB_SUCCESS) {
		fprintf(stderr, "libusb_set_option(LIBUSB_OPTION_USE_USBDK): %s\n", libusb_strerror(rc));
	}
#endif

	if (usb_log_level >= LIBUSB_LOG_LEVEL_NONE && usb_log_level <= LIBUSB_LOG_LEVEL_DEBUG) {
		rc = libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, usb_log_level);
		if (rc != LIBUSB_SUCCESS) {
			fprintf(stderr, "libusb_set_option(LIBUSB_OPTION_LOG_LEVEL): %s\n", libusb_strerror(rc));
		}
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

static int usb_read(void *hndl, unsigned char id, void *buf, unsigned size)
{
	struct usb_context *usb = hndl;
	int rc, trans_number;

	rc = libusb_interrupt_transfer(usb->hndl, id, buf, size, &trans_number, USB_TRANS_TIMEOUT);
	if (rc != 0) {
		return -1;
	}

	return trans_number;
}

static int usb_write(void *hndl, unsigned char id, const void *buf, unsigned size)
{
	struct usb_context *usb = hndl;
	int rc, trans_number;

	rc = libusb_interrupt_transfer(usb->hndl, id, (void*)buf, size, &trans_number, USB_TRANS_TIMEOUT);
	if (rc != 0) {
		fprintf(stderr, "libusb_write: %s\n", libusb_strerror(rc));
		return -1;
	}

	return trans_number;
}

static void usb_close(void *user_data)
{
	struct usb_context *usb = user_data;

	if (usb->flags & FLAG_AUTO_DETACH_KERNEL_DRIVER) {
		libusb_release_interface(usb->hndl, usb->iface);
	}
	libusb_close(usb->hndl);
	libusb_exit(NULL);

	free(usb);
}

struct transport *usb_transport_open(uint16_t vid, uint16_t pid, int iface, unsigned flags)
{
	int rc;
	uint32_t baudrate = 115200;
	libusb_device_handle *hndl;
	struct usb_context *usb;

	usb_init(LIBUSB_LOG_LEVEL_NONE);
	hndl = usb_open_timeout(vid, pid, iface, 10 * 1000, &rc, flags);
	if (hndl == NULL) {
		libusb_exit(NULL);
		return NULL;
	}

	usb = malloc(sizeof(struct usb_context));
	usb->hndl = hndl;
	usb->flags = flags;
	usb->iface = iface;

	return mcu_transport_open(usb, usb_close, usb_read, usb_write);
}
