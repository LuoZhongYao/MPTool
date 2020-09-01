#include "defs.h"
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "transport.h"
#include "mcu_transport.h"

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

struct mcu_transport {
	void *hndl;
	void (*close)(void *hndl);
	int (*read)(void *hndl, unsigned char id, void *buf, unsigned size);
	int (*write)(void *hndl, unsigned char id, const void *buf, unsigned size);
	struct transport transport;
};


static unsigned char checksum(unsigned char *data, unsigned size)
{
	int i;
	unsigned char sum = 0;

	for (i = 0; i < size; i++) {
		sum += data[i];
	}

	return sum;
}

static int mcu_write_command(struct mcu_transport *trans, uint8_t cmd, const void *param, uint8_t size, unsigned timeout)
{
	int rc;
	int trans_number;
	int retry = 10;
	uint8_t tmp[64];
	uint8_t rsp[64];

	memset(tmp, 0, 64);

	tmp[0] = 0x03;
	tmp[1] = cmd;
	tmp[2] = size;
	memcpy(tmp + 3, param, size);
	tmp[63] = checksum(tmp, 63);

	rc = trans->write(trans->hndl, 0x02, tmp, 64);

	if (rc <= 0) {
		return rc;
	}

	while (retry--) {
		rc = trans->read(trans->hndl, 0x81, rsp, 64);
		if (rc <= 0) {
			return rc;
		}

		if (rc != 64 || rsp[0] != 0x01) {
			return -1;
		}

		if (rsp[1] == 0 && rsp[2] == 2 && rsp[3] == cmd && rsp[4] == 0) {
			return 0;
		}
	}

	return -1;
}

static int mcu_read_block(struct mcu_transport *trans, void *buf, unsigned size, unsigned *read_size, unsigned timeout)
{
	int rc;
	int sum;
	uint8_t crc;
	uint8_t tmp[64];
	uint8_t rsp[64];

	memset(tmp, 0, 64);
	tmp[0] = 0x03;
	tmp[1] = USB_TRANS_CMD_READ;
	tmp[2] = 1;
	tmp[3] = size;
	tmp[63] = checksum(tmp, 63);

	*read_size = 0;
	rc = trans->write(trans->hndl, 2, tmp, 64);
	if (rc != 64) {
		return -1;
	}

	rc = trans->read(trans->hndl, 0x81, rsp, 64);
	if (rc != 64) {
		return -1;
	}

	if (rsp[0] != 0x01) {
		return -1;
	}

	//sum = checksum(rsp, 63);
	//if (sum != rsp[63]) {
	//	return LIBUSB_ERROR_OTHER + 0x01;
	//}

	if (rsp[1] == 0x00 && rsp[2] == 2 && rsp[3] == 0x04) {
		return -1 - rsp[4];
	}

	if (rsp[1] != 0x04) {
		return -1;
	}

	if (rsp[2] > size) {
		return -1;
	}

	memcpy(buf, rsp + 3, rsp[2]);
	*read_size = rsp[2];

	return 0;
}

static int mcu_write(struct transport *trans, const void *buf, unsigned size)
{
	int rc;
	unsigned write_number = 0;
	struct mcu_transport *mcu = container_of(trans, struct mcu_transport, transport);

	while (write_number < size) {
		unsigned count = MIN(size - write_number, TRANS_BLOCK_SIZE);

		rc = mcu_write_command(mcu, USB_TRANS_CMD_WRITE,
			buf + write_number, count, USB_WRITE_TIMEOUT);
		if (rc != 0) {
			return write_number;
		}
		write_number += count;
	}

	return write_number;
}

static int mcu_read(struct transport *trans, void *buf, unsigned size)
{
	int rc;
	int retry = 300;
	unsigned read_number = 0;
	struct mcu_transport *mcu = container_of(trans, struct mcu_transport, transport);

	while (read_number < size) {
		unsigned bytes = 0;
		unsigned count = MIN(size - read_number, TRANS_BLOCK_SIZE);

		rc = mcu_read_block(mcu, buf + read_number, count, &bytes, USB_READ_TIMEOUT);
		if (rc != 0) {
			return read_number;
		}

		if (bytes == 0) {
			if (retry-- <= 0) {
				errno = ETIMEDOUT;
				return read_number;
			}

			usleep(1000);
		}

		read_number += bytes;
	}

	return read_number;
}

static void mcu_close(struct transport *trans)
{
	struct mcu_transport *mcu = container_of(trans, struct mcu_transport, transport);

	mcu_write_command(mcu, USB_TRANS_CMD_FINISH,
		NULL, 0, USB_START_TIMEOUT);
	if (mcu->close) {
		mcu->close(mcu->hndl);
	}

	free(mcu);
}

static int mcu_set_baudrate(struct transport *trans, unsigned speed)
{
	uint32_t baudrate = speed;
	struct mcu_transport *mcu = container_of(trans, struct mcu_transport, transport);

	return mcu_write_command(mcu, USB_TRANS_CMD_SET_BAUDRATE,
		&baudrate, 4, USB_WRITE_TIMEOUT);
}

static const struct transport_ops mcu_transport_ops = {
	.write = mcu_write,
	.read = mcu_read,
	.close = mcu_close,
	.set_baudrate = mcu_set_baudrate,
};

struct transport *mcu_transport_open(void *hndl,
	void (*close)(void *hndl),
	int (*read)(void *hndl, unsigned char id, void *buf, unsigned size),
	int (*write)(void *hndl, unsigned char id, const void *buf, unsigned size))
{
	int rc;
	uint32_t baudrate = 115200;
	struct mcu_transport *mcu;

	mcu = malloc(sizeof(struct mcu_transport));
	mcu->hndl = hndl;
	mcu->read = read;
	mcu->write = write;
	mcu->close = close;
	mcu->transport.ops = &mcu_transport_ops;

	rc = mcu_write_command(mcu, USB_TRANS_CMD_START, &baudrate, 4, USB_START_TIMEOUT);
	if (rc != 0) {
		fprintf(stderr, "start MP failure: %s\n", strerror(errno));
		mcu_close(&mcu->transport);
		return NULL;
	}

	return &mcu->transport;
}
