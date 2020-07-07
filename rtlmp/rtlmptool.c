/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include "crc16.h"
#include "rtlmp.h"
#include "rtlbt.h"
#include "rtlimg.h"
#include "rtlmptool.h"
#include "transport.h"
#include <stdio.h>
#include <string.h>

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

	do {
		if (1 != read_bytes(ev, 1)) {
			return -1;
		}
	} while (ev[0] != 0x04);

	if (2 != read_bytes(ev + 1, 2))  {
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

	if ((4 + size) != transport_write(trans, hdr, 4 + size)) {
	}
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

static void rtlmp_read_x00(void)
{
	uint8_t buf[70];
	read_bytes(buf, sizeof(buf));
}

int rtlmptool_download_firmware(void *trns, int speed,
	const char *fw, const char *mp, int *progress)
{
	int rc, fw_size = 0, mp_size = 0;
	FILE *fpw, *fpm;

	trans = trns;

	fpw = fopen(fw, "rb");
	if (fpw == NULL) {
		return -1;
	}

	fw_size = rtlbt_cacl_download_size(fpw);

	fpm = fopen(mp, "rb");
	if (fpm == NULL) {
		fclose(fpw);
		return -1;
	}

	mp_size = rtlimg_calc_download_size(fpm);
	if (mp_size < 0) {
		fclose(fpw);
		return -1;
	}

	rtlbt_read_chip_type();
	rtlbt_vendor_cmd62((uint8_t[]){0x20, 0xa8, 0x02, 0x00, 0x40,
		0x04, 0x02, 0x00, 0x01});

	rtlbt_fw_download(fpw, fw_size + mp_size, 0, progress);
	rtlbt_vendor_cmd62((uint8_t[]){0x20, 0x34, 0x12, 0x20, 0x00,
		0x31, 0x38, 0x20, 0x00});

	rtlmp_read_x00();
	rc = rtlmp_change_baudrate(speed);
	if (rc != 0) {
		fprintf(stderr, "change baudrate %d failure, rc = %d\n", speed, rc);
		goto _quit;
	}

	transport_set_baudrate(trans, speed);
	rc = rtlimg_download(fpm, fw_size + mp_size, fw_size, progress);
	if (rc != 0) {
		fprintf(stderr, "rtlimg download failure, rc = %d\n", rc);
		goto _quit;
	}
	rtlmp_reset(0x01);

_quit:
	fclose(fpm);
	fclose(fpw);
	return rc;
}

