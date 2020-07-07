/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include "rtlmp.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int rtlmp_read(void *mp, uint32_t size);
int rtlmp_write(const void *mp, uint32_t size);
int rtlmp_send_sync(const void *mp, uint32_t size, void *rsp, uint32_t rsp_size);
uint16_t crc16_check(uint8_t *buf, uint16_t len, uint16_t value);

struct mpbaudrate_cp {
	uint8_t magic;
	uint16_t command;
	uint32_t baudrate;
	uint8_t padding;
} __attribute__((packed));

struct mpreset_cp {
	uint8_t magic;
	uint16_t command;
	uint8_t mode;
} __attribute__((packed));

struct mpflash_cp {
	uint8_t magic;
	uint16_t command;
	uint32_t addr;
	uint32_t size;
} __attribute__((packed));

struct mpcommon_rp {
  uint8_t magic;
  uint16_t command;
  uint8_t padding;
  uint32_t length;
} __attribute__((packed));

int rtlmp_reset(uint8_t mode)
{
	struct mpreset_cp cp;
	struct mpcommon_rp rp;

	cp.magic = 0x87;
	cp.command = 0x1041;
	cp.mode = mode;
	return rtlmp_send_sync(&cp, sizeof(cp), &rp, sizeof(rp));
}

int rtlmp_change_baudrate(uint32_t baudrate)
{
	struct mpcommon_rp rp;
	struct mpbaudrate_cp cp;

	cp.magic = 0x87;
	cp.command = 0x1010;
	cp.baudrate = baudrate;
	cp.padding = 0xff;

	return rtlmp_send_sync(&cp, sizeof(cp), &rp, sizeof(rp));
}

int rtlmp_erase_flash(uint32_t addr, uint32_t size)
{
	struct mpflash_cp cp;
	struct mpcommon_rp rp;

	cp.magic = 0x87;
	cp.command = 0x1030;
	cp.addr = addr;
	cp.size = size;

	return rtlmp_send_sync(&cp, sizeof(cp), &rp, sizeof(rp));
}

int rtlmp_write_flash(uint32_t addr, uint32_t size, const void *dat)
{
	int rs;
	struct mpflash_cp *cp = malloc(sizeof(*cp) + size);
	struct mpcommon_rp rp;

	cp->magic = 0x87;
	cp->command = 0x1032;
	cp->addr = addr;
	cp->size = size;
	memcpy(((void*)cp) + sizeof(*cp), dat, size);

	rs = rtlmp_send_sync(cp, sizeof(*cp) + size, &rp, sizeof(rp));
	free(cp);

	return rs;
}

int rtlmp_read_flash(uint32_t addr, uint32_t size, void *dat)
{
	int rs;
	struct mpflash_cp cp;
	struct mpcommon_rp *rp = malloc(sizeof(*rp) + size);

	cp.magic = 0x87;
	cp.command = 0x1032;
	cp.addr = addr;
	cp.size = size;

	rs = rtlmp_send_sync(&cp, sizeof(cp), rp, sizeof(*rp) + size);
	free(rp);

	return rs;
}

int rtlmp_verify_flash(uint32_t addr, uint32_t size, uint16_t crc)
{
	int rs;
	struct mpflash_cp *cp = malloc(sizeof(*cp) + 2);
	struct mpcommon_rp rp;

	cp->magic = 0x87;
	cp->command = 0x1050;
	cp->addr = addr;
	cp->size = size;
	memcpy(((void*)cp) + sizeof(*cp), &crc, 2);

	rs = rtlmp_send_sync(cp, sizeof(*cp) + 2, &rp, sizeof(rp));
	free(cp);

	return rs;
}
