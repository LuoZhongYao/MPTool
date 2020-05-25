/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */


#ifndef __RTLBT_H__
#define __RTLBT_H__

int rtlbt_fw_download(const char *fw);
int rtlbt_change_baudrate(unsigned baudrate);
int rtlbt_vendor_cmd62(const unsigned char dat[9]);
int rtlbt_read_chip_type(void);


#endif /* __RTLBT_H__*/

