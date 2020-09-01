/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */


#ifndef __RTLBT_H__
#define __RTLBT_H__

#include <stdio.h>

int rtlbt_single_tone(unsigned char ch);
int rtlbt_cacl_download_size(FILE *fd);
int rtlbt_change_baudrate(unsigned baudrate);
int rtlbt_vendor_cmd62(const unsigned char dat[9]);
int rtlbt_read_chip_type(void);
int rtlbt_fw_download(FILE *fd, int total, int dwsized, int *progress);

#endif /* __RTLBT_H__*/

