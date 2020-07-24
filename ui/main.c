#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include "rtlmptool.h"
#include "transport.h"

struct transport *trans;
struct transport *hidapi_transport_open(uint16_t vid, uint16_t pid);
struct transport *serial_transport_open(const char *dev, unsigned speed);
struct transport *usb_transport_open(uint16_t vid, uint16_t pid, int iface, unsigned flags);

#define TRANS_IFACE_NONE	0x00
#define TRANS_IFACE_SERIAL	0x01
#define TRANS_IFACE_USB		0x02
#define TRANS_IFACE_HID		0x03

#define check_and_set_trans_iface(trans_iface, iface)	do {								\
	if (trans_iface != TRANS_IFACE_NONE && trans_iface != iface) {							\
		fprintf(stderr, "can't set multiple transmission interfaces at the same time\n");	\
		usage(1);																			\
	}																						\
	trans_iface = iface;																	\
} while(0)

static void usage(int rc)
{
	exit(rc);
}

int main(int argc, char **argv)
{
	int c, rc;
	int iface = 0, flags = 0, trans_iface = TRANS_IFACE_NONE;
	uint16_t vid = 0x3285, pid = 0x0609;
	unsigned speed = 115200;
	const char *tty = "/dev/ttyS0";
	const char *fw = "firmware0.bin";
	const char *mp = "app.bin";

	while (-1 != (c = getopt(argc, argv, "b:f:m:U:T:H:kh"))) {
		switch (c) {
		case 'k': flags |= 0x0001; break;
		case 'T':  {
			tty = optarg;
			check_and_set_trans_iface(trans_iface, TRANS_IFACE_SERIAL);
		} break;

		case 'H': {
			check_and_set_trans_iface(trans_iface, TRANS_IFACE_HID);
			sscanf(optarg, "%04hx:%04hx", &vid, &pid);
		} break;

		case 'U': {
			check_and_set_trans_iface(trans_iface, TRANS_IFACE_USB);
			sscanf(optarg, "%04hx:%04hx,%d", &vid, &pid, &iface);
		} break;
		case 'b': speed = strtol(optarg, NULL, 0); break;
		case 'f': fw = optarg; break;
		case 'm': mp = optarg; break;
		case 'h': usage(0); break;
		default: usage(1); break;
		}
	}

	switch (trans_iface) {
	case TRANS_IFACE_NONE:
	case TRANS_IFACE_SERIAL:
		trans = serial_transport_open(tty, 115200);
	break;

	case TRANS_IFACE_USB:
		trans = usb_transport_open(vid, pid, iface, flags);
	break;

	case TRANS_IFACE_HID:
		trans = hidapi_transport_open(vid, pid);
	break;
	}

	if(trans == NULL) {
		fprintf(stderr, "Transport interface (%04x:%04x,%d) or (%s) %s\n",
			vid, pid, iface, tty, strerror(errno));
		exit(1);
	}

	rc = rtlmptool_download_firmware(trans, speed, fw, mp, NULL);
	if (rc != 0) {
		fprintf(stderr, "donwload firmware failure: %s\n", strerror(errno));
	}
	transport_close(trans);

	return rc;
}
