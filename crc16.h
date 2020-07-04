/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */


#ifndef __CRC16_H__
#define __CRC16_H__

#include <stdint.h>
#include <stdbool.h>

uint16_t crc16_check(uint8_t *buf, uint16_t len, uint16_t value);

#endif /* __CRC16_H__*/

