#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <stdbool.h>
#include <getopt.h>
#include "rtlmp.h"
#include "rtlbt.h"
#include "rtlimg.h"
#include "transport.h"

#define HCI_COMMAND_PKT     0x01
#define HCI_ACLDATA_PKT     0x02
#define HCI_SCODATA_PKT     0x03
#define HCI_EVENT_PKT       0x04
#define HCI_DIAG_PKT        0xf0
#define HCI_VENDOR_PKT      0xff

#define HCI_COMMAND_HDR_SIZE 3
#define HCI_EVENT_HDR_SIZE   2
#define HCI_ACL_HDR_SIZE     4
#define HCI_SCO_HDR_SIZE     3

#define HCI_MAX_ACL_SIZE    1024
#define HCI_MAX_SCO_SIZE    255
#define HCI_MAX_COMMAND_SIZE 260
#define HCI_MAX_EVENT_SIZE   260
#define HCI_MAX_FRAME_SIZE  (HCI_MAX_ACL_SIZE + 4)

static struct transport *trans;
uint16_t crc16_check(uint8_t *buf, uint16_t len, uint16_t value)
{

    uint16_t b = 0xA001;
    bool reminder = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        value ^= buf[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            reminder = value % 2;
            value >>= 1;
            if (reminder == 1)
            {
                value ^= b;
            }
        }
    }
    return value;
}

static int read_bytes(void *buf, uint16_t size)
{
	int reqsz;

	for (reqsz = 0; reqsz < size; ) {
		int rz = transport_read(trans, buf + reqsz, size - reqsz);
		if (rz < 0) {
			return -1;
		}
		reqsz += rz;
	}

	return reqsz;
}

int hci_read(void *buf, uint16_t size)
{
	uint8_t ev[256 + 3];

	if (size < 3) {
		return -1;
	}

	if (3 != read_bytes(ev, 3))  {
		return -1;
	}

	if (ev[2] != read_bytes(ev + 3, ev[2])) {
		return -1;
	}

	memcpy(buf, ev, size);

	return size;
}

void hci_send_cmd(uint16_t opcode, const void *params, uint8_t size)
{
	uint8_t hdr[260];

	hdr[0] = 0x01;
	hdr[1] = opcode & 0xff;
	hdr[2] = (opcode >> 8) & 0xff;
	hdr[3] = size;

	if (params && size) {
		memcpy(hdr + 4, params, size);
	}

	transport_write(trans, hdr, 4 + size);
}

int hci_send_cmd_sync(uint16_t opcode, const void *params, uint8_t size,
	void *rsp, uint16_t rsp_size)
{
	uint16_t sz;
	uint8_t ev[256 + 3];
	hci_send_cmd(opcode, params, size);

	do {
		sz = hci_read(ev, rsp_size + 6);
		//printf("sz: %d, %02x %02x %02x %02x %02x %02x\n", sz, ev[0], ev[1], ev[2], ev[3], ev[4], ev[5]);
	} while (ev[0] != 0x04 || ev[1] != 0x0e || opcode != (ev[4] | ev[5] << 8));

	if (rsp && rsp_size) {
		memcpy(rsp, ev + 6, rsp_size);
	}

	return 0;
}

int rtlmp_write(const void *mp, uint32_t size)
{
	uint16_t crc;
	uint8_t buf[size + 2];

	crc = crc16_check((uint8_t *)mp, size, 0);
	memcpy(buf, mp, size);
	memcpy(buf + size, &crc, 2);

	transport_write(trans, buf, size + 2);
	return 0;
}

int rtlmp_read(void *mp, uint32_t size)
{
	uint16_t crc;
	uint8_t buf[size + 2];

	read_bytes(buf, size + 2);
	memcpy(mp, buf, size);

	crc = buf[size] | (buf[size + 1] << 8);

	return crc == crc16_check(mp, size, 0);
}

int rtlmp_send_sync(const void *mp, uint32_t size, void *rsp, uint32_t rsp_size)
{
	rtlmp_write(mp, size);
	return rtlmp_read(rsp, rsp_size) ? 0 : -1;
}

void rtlmp_read_x00(void)
{
	uint8_t buf[70];
	read_bytes(buf, sizeof(buf));
}

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

	//ufd = uart_open(tty, 115200);
	//rtlbt_change_baudrate(speed);
	//usleep(1000 * 1000);
	//uart_set_baudrate(ufd, speed);
	//usleep(1000 * 1000);

	rtlbt_read_chip_type();

	rtlbt_vendor_cmd62((uint8_t[]){0x20, 0xa8, 0x02, 0x00, 0x40,
		0x04, 0x02, 0x00, 0x01});

	rtlbt_fw_download(fw);

	rtlbt_vendor_cmd62((uint8_t[]){0x20, 0x34, 0x12, 0x20, 0x00,
		0x31, 0x38, 0x20, 0x00});

	rtlmp_read_x00();
	rc = rtlmp_change_baudrate(speed);
	if (rc != 0) {
		fprintf(stderr, "change baudrate %d failure, rc = %d\n", speed, rc);
		goto _quit;
	}

	transport_set_baudrate(trans, speed);
	rc = rtlimg_download(mp);
	if (rc != 0) {
		fprintf(stderr, "rtlimg download failure, rc = %d\n", rc);
		goto _quit;
	}
	rtlmp_reset(0x01);

_quit:
	transport_close(trans);

	return rc;
}
