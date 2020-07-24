/*
 * Copyright (c) 2020 ZhongYao Luo <luozhongyao@gmail.com>
 * 
 * SPDX-License-Identifier: 
 */

#include "defs.h"
#include "transport.h"
#include <windows.h>


struct com_transport {
	HANDLE hndl;
	struct transport transport;
};

static int com_read(struct transport *trans, void *buf, unsigned size)
{
	DWORD rn;
	struct com_transport *com = container_of(trans, struct com_transport, transport);

	if (ReadFile(com->hndl, buf, size, &rn, NULL)) {
		return rn;
	}

	return -1;
}

static int com_write(struct transport *trans, const void *buf, unsigned size)
{
	DWORD wn;
	struct com_transport *com = container_of(trans, struct com_transport, transport);

	if (WriteFile(com->hndl, buf, size, &wn, NULL)) {
		return wn;
	}

	return -1;
}

static void com_close(struct transport *trans)
{
	struct com_transport *com = container_of(trans, struct com_transport, transport);

	CloseHandle(com->hndl);
	free(com);
}

static int com_set_baudrate(struct transport *trans, unsigned speed)
{
	DCB dcb;
	struct com_transport *com = container_of(trans, struct com_transport, transport);

	GetCommState(com->hndl, &dcb);
	dcb.BaudRate = speed;
	SetCommState(com->hndl, &dcb);

	PurgeComm(com->hndl, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

	return 0;
}

static const struct transport_ops com_ops = {
	.read = com_read,
	.write = com_write,
	.set_baudrate = com_set_baudrate,
	.close = com_close,
};

struct transport *serial_transport_open(const char *dev, unsigned speed)
{
	DCB dcb;
	HANDLE hndl;
	struct com_transport *com;

	hndl = CreateFile(dev, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hndl == INVALID_HANDLE_VALUE) {
		errno = ENODEV;
		return NULL;
	}

	GetCommState(hndl, &dcb);
	dcb.fParity = NOPARITY;
	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.BaudRate = speed;
	if (!SetCommState(hndl, &dcb)) {
		CloseHandle(hndl);
		errno = EINVAL;
		return NULL;
	}

	PurgeComm(hndl, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

	com = malloc(sizeof(struct com_transport));
	com->hndl = hndl;
	com->transport.ops = &com_ops;

	return &com->transport;
}
