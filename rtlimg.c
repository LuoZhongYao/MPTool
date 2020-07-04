/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include "defs.h"
#include "rtlmp.h"
#include "rtlimg.h"
#include <stdio.h>
#include <errno.h>

struct dwhdr {
	uint32_t dw_addr;
	uint32_t dw_size;
};

uint16_t crc16_check(uint8_t *buf, uint16_t len, uint16_t value);
static void parse_mp(struct mphdr *hdr, struct dwhdr *dw)
{
	uint8_t *buf = (uint8_t *)(hdr + 1);
	switch (hdr->id) {
	case 4:
	case 20:
		if (hdr->length == 4) {
			dw->dw_size = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
		}
	break;

	case 19:
		if (hdr->length == 4) {
			dw->dw_addr = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
		}
	break;

	}
}

static int slice_download(uint32_t addr, uint8_t *buf, uint32_t size)
{
	int rs = 0;
	uint32_t wn = 0;

	while (wn < size)  {
		int c = MIN(2048, size - wn);
		rs = rtlmp_write_flash(addr + wn, c, buf + wn);
		if (rs < 0)
			break;
		wn += c;
	}

	return rs;
}

static int do_download(FILE *fd, struct dwhdr *dw)
{
	int rs = -1;
	uint8_t dat[4096];
	uint16_t crc16;
	uint32_t dwsz = 0;

	while (dwsz < dw->dw_size) {
		int rz = fread(dat, 1, MIN(sizeof(dat), dw->dw_size - dwsz), fd);
		if (rz <= 0)
			break;

		rs = rtlmp_erase_flash(dw->dw_addr + dwsz, sizeof(dat));
		if (rs < 0)
			break;

		crc16 = crc16_check(dat, rz, 0);

		rs = slice_download(dw->dw_addr + dwsz, dat, rz);

		if (rs < 0)
			break;

		rs = rtlmp_verify_flash(dw->dw_addr + dwsz, rz, crc16);
		if (rs < 0)
			break;

		dwsz += rz;
	}

	return rs;
}

static int do_img_download(FILE *fd, uint32_t off, uint32_t addr)
{
	int i = 0;
	uint8_t buf[512];
	struct dwhdr dw;
	struct mphdr *hdr;

	fseek(fd, off, SEEK_SET);
	if (sizeof(buf) != fread(buf, 1, sizeof(buf), fd)) {
		return -1;
	}

	dw.dw_addr = addr;
	for (i = 0; i < sizeof(buf);) {
		hdr = (struct mphdr*)(buf + i);

		//printf("i: %d, id: %x, length: %x\n", i, hdr->id, hdr->length);
		if (hdr->id <= 0 || hdr->id >= 255)
			break;

		if (hdr->length == 0)
			continue;

		parse_mp(hdr, &dw);

		i += 3;
		if (i + hdr->length > sizeof(buf))
			break;

		i += hdr->length;
	}

	printf("Download: %x, %x\n", dw.dw_addr, dw.dw_size);
	return do_download(fd, &dw);
}

int rtlimg_download(const char *img)
{
	int i;
	FILE *fd;
	unsigned nr;
	uint32_t off;
	struct imghdr hdr;
	struct subhdr sub[32];
	int rs = -1;

	fd = fopen(img, "rb");
	if (fd == NULL) {
		return -1;
	}

	if (sizeof(hdr) != fread(&hdr, 1, sizeof(hdr), fd)) {
		errno = EINVAL;
		goto _quit;
	}

	if (hdr.sign != 0x4d47) {
		errno = EINVAL;
		goto _quit;
	}

	nr = __builtin_popcount(hdr.subFileIndicator);
	for (i = 0;i < nr; i++) {
		if (sizeof(struct subhdr) != fread(&sub[i], 1, sizeof(struct subhdr), fd)) {
			errno = EINVAL;
			goto _quit;
		}
	}

	off = sizeof(hdr) + nr * sizeof(struct subhdr);
	for (i = 0; i < nr; i++) {
		if (do_img_download(fd, off, sub[i].downloadAddr)) {
			fprintf(stderr, "Download failure: offset = %x, addresss %x\n", off, sub[i].downloadAddr);
		}
		off += sub[i].size;
	}

	rs = 0;
_quit:
	fclose(fd);
	return rs;
}
