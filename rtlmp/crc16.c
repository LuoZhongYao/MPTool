/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include "crc16.h"

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


