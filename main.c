#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <limits.h>
#include <termios.h>
#include "baudrate.h"
#include "rtlmp.h"
#include "rtlbt.h"
#include "rtlimg.h"

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

static speed_t tty_get_speed(int speed)
{
#define _(n) case n: return B ##n
	switch (speed) {
		_(9600); break;
		_(19200); break;
		_(38400); break;
		_(57600); break;
		_(115200); break;
		_(230400); break;
		_(460800); break;
		_(500000); break;
		_(576000); break;
		_(921600); break;
		_(1000000); break;
		_(1152000); break;
		_(1500000); break;
		_(2000000); break;
		_(2500000); break;
		_(3000000); break;
		_(3500000); break;
		_(4000000); break;
	default:
		fprintf(stderr, "Not support %d\n", speed);
	break;
	}
#undef _
	return -1;
}

static int uart_open(const char *dev, int speed)
{
	int fd;
	speed_t baudrate;
	struct termios ti;
	struct serial_struct serial;

	if (dev == NULL)
		return -1;

	fd = open(dev, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror(dev);
		exit(1);
	}

	if (!isatty(fd))
		return fd;

	tcflush(fd, TCIOFLUSH);

	if (tcgetattr(fd, &ti) < 0) {
		perror("get port settings");
		exit(1);
	}
	cfmakeraw(&ti);
	ti.c_cflag |= CLOCAL;
	ti.c_cflag &= ~CRTSCTS;
	ti.c_cflag &= ~CSTOPB;
	ti.c_cflag |= CS8;
	ti.c_cflag &= ~PARENB;
	ti.c_cc[VMIN] = 1;
	ti.c_cc[VTIME] = 0;
	baudrate = tty_get_speed(speed);
	if (baudrate != -1) {
		cfsetispeed(&ti, baudrate);
		cfsetospeed(&ti, baudrate);
	}

	if (tcsetattr(fd, TCSANOW, &ti) < 0) {
		perror("set port settings");
		exit(1);
	}

	tcflush(fd, TCIOFLUSH);
	if (baudrate == -1) {
		if (set_baudrate(fd, speed)) {
			perror("set baudrate");
			exit(1);
		}
	}

	if (ioctl(fd, TIOCGSERIAL, &serial) == 0) {
		serial.flags |= ASYNC_LOW_LATENCY;
		if (ioctl(fd, TIOCSSERIAL, &serial)) {
			perror("set serial");
		}
	}

	return fd;
}

void uart_set_baudrate(int fd, unsigned speed)
{
	speed_t baudrate;
	struct termios ti;

	tcgetattr(fd, &ti);
	baudrate = tty_get_speed(speed);
	if (baudrate != -1) {
		cfsetispeed(&ti, baudrate);
		cfsetospeed(&ti, baudrate);
	}

	if (tcsetattr(fd, TCSANOW, &ti) < 0) {
		perror("set port settings");
		exit(1);
	}

	tcflush(fd, TCIOFLUSH);
	if (baudrate == -1) {
		if (set_baudrate(fd, speed)) {
			perror("set baudrate");
			exit(1);
		}
	}
}

static int ufd = -1;
int hci_write(const uint8_t hdr[4], const void *params, uint8_t size)
{
	write(ufd, hdr, 4);
	write(ufd, params, size);

	return 0;
}

static int read_bytes(void *buf, uint16_t size)
{
	int reqsz;

	for (reqsz = 0; reqsz < size; ) {
		int rz = read(ufd, buf + reqsz, size - reqsz);
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
	const uint8_t hdr [] = {
		0x01,
		opcode & 0xff,
		(opcode >> 8) & 0xff,
		size,
	};

	hci_write(hdr, params, size);
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

	write(ufd, mp, size);
	crc = crc16_check((uint8_t *)mp, size, 0);
	write(ufd, &crc, 2);

	return 0;
}

int rtlmp_read(void *mp, uint32_t size)
{
	uint16_t crc;
	read_bytes(mp, size);
	read_bytes(&crc, 2);

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

int main(int argc, char **argv)
{
	int c;

	unsigned speed = 115200;
	const char *tty = "/dev/ttyS0";
	const char *fw = "firmware0.bin";
	const char *mp = "app.bin";

	while (-1 != (c = getopt(argc, argv, "d:b:f:m:"))) {
		switch (c) {
		case 'd': tty = optarg; break;
		case 'b': speed = strtol(optarg, NULL, 0); break;
		case 'f': fw = optarg; break;
		case 'm': mp = optarg; break;
		}
	}

	ufd = uart_open(tty, 115200);
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
	rtlmp_change_baudrate(speed);
	uart_set_baudrate(ufd, speed);

	rtlimg_download(mp);
	rtlmp_reset(0x01);

	return 0;
}
