/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */


#ifndef __RTLMP_H__
#define __RTLMP_H__

#include <stdint.h>

int rtlmp_reset(uint8_t mode);
int rtlmp_change_baudrate(uint32_t baudrate);
int rtlmp_erase_flash(uint32_t addr, uint32_t size);
int rtlmp_write_flash(uint32_t addr, uint32_t size, const void *dat);
int rtlmp_verify_flash(uint32_t addr, uint32_t size, uint16_t crc16);

#endif /* __RTLMP_H__*/

