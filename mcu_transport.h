/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#ifndef __MCU_TRANSPORT_H__
#define __MCU_TRANSPORT_H__

#ifdef __cplusplus
extern "C" {
#endif

struct transport;
struct transport *mcu_transport_open(void *hndl,
	int (*read)(void *hndl, unsigned char id, void *buf, unsigned size),
	int (*write)(void *hndl, unsigned char id, const void *buf, unsigned size));

#ifdef __cplusplus
}
#endif


#endif /* __MCU_TRANSPORT_H__*/

