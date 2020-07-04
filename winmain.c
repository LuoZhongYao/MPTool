/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include <stdint.h>
#include "transport.h"
#include "rtlmptool.h"
struct transport *hidapi_transport_open(uint16_t vid, uint16_t pid);

int WinMain(int argc, char **argv)
{
	uint16_t vid = 0x3285, pid = 0x0609;
	const char *fw = "firmware0.bin";
	const char *mp = "MP_LBA1127.bin";
	struct transport *trans;

	trans = hidapi_transport_open(vid, pid);

	rtlmptool_download_firmware(trans, 115200, fw, mp);

	return 0;
}
