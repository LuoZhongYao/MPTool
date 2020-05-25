/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */


#ifndef __RTLIMG_H__
#define __RTLIMG_H__

#include <stdint.h>

struct imghdr {
	uint16_t sign;
	uint32_t sizeOfMergedFile;
	uint8_t checkSum[32];
	uint8_t revision;
	uint8_t icType;
	uint32_t subFileIndicator;
} __attribute__((packed));

struct subhdr {
	uint32_t downloadAddr;
	uint32_t size;
	uint32_t reserved;
} __attribute__((packed));

struct mphdr {
	int16_t id;
	uint8_t length;
} __attribute__((packed));

int rtlimg_download(const char *img);

#endif /* __RTLIMG_H__*/

