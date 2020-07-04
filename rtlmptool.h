/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */


#ifndef __RTLMPTOOL_H__
#define __RTLMPTOOL_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int rtlmptool_download_firmware(void *trns, int speed, const char *fw, const char *mp);

#ifdef __cplusplus
}
#endif

#endif /* __RTLMPTOOL_H__*/

