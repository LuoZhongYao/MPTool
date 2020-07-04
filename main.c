#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <stdbool.h>
#include <getopt.h>
#include "rtlmptool.h"
#include "transport.h"

struct transport *trans;
struct transport *serial_transport_open(const char *dev, unsigned speed);
struct transport *usb_transport_open(uint16_t vid, uint16_t pid, int iface, unsigned flags);

#define TRANS_IFACE_NONE	0x00
#define TRANS_IFACE_SERIAL	0x01
#define TRANS_IFACE_USB		0x02

#define check_and_set_trans_iface(trans_iface, iface)	do {								\
	if (trans_iface != TRANS_IFACE_NONE && trans_iface != iface) {							\
		fprintf(stderr, "can't set multiple transmission interfaces at the same time\n");	\
		exit(1);																			\
	}																						\
	trans_iface = iface;																	\
} while(0)

int main(int argc, char **argv)
{
	int c, rc;
	int iface = 0, flags = 0, trans_iface = TRANS_IFACE_NONE;
	uint16_t vid, pid;
	unsigned speed = 115200;
	const char *tty = "/dev/ttyS0";
	const char *fw = "firmware0.bin";
	const char *mp = "app.bin";

	while (-1 != (c = getopt(argc, argv, "t:b:f:m:u:k"))) {
		switch (c) {
		case 'k': flags |= 0x0001; break;
		case 'u': {
			check_and_set_trans_iface(trans_iface, TRANS_IFACE_USB);
			sscanf(optarg, "%04hx:%04hx,%d", &vid, &pid, &iface);
		} break;
		case 't':  {
			tty = optarg;
			check_and_set_trans_iface(trans_iface, TRANS_IFACE_SERIAL);
		} break;
		case 'b': speed = strtol(optarg, NULL, 0); break;
		case 'f': fw = optarg; break;
		case 'm': mp = optarg; break;
		}
	}

	if (TRANS_IFACE_NONE == trans_iface || TRANS_IFACE_SERIAL == trans_iface) {
		trans = serial_transport_open(tty, 115200);
	} else if (TRANS_IFACE_USB == trans_iface) {
		trans = usb_transport_open(vid, pid, iface, flags);
	}

	if(trans == NULL) {
		fprintf(stderr, "can't open transport interfaces VID:%04x, PID:%04x, IFACE:%d %s\n",
			vid, pid, iface, strerror(errno));
		exit(1);
	}

	return rtlmptool_download_firmware(trans, speed, fw, mp);
}
