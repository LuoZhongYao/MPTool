/*
 * Written by ZhongYao Luo <luozhongyao@gmail.com>
 *
 * Copyright 2020 ZhongYao Luo
 */

#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

#include <errno.h>

struct transport;
struct transport_ops {
	int (*set_baudrate)(struct transport *trans, unsigned speed);
	int (*read)(struct transport *trans, void *buf, unsigned size);
	int (*write)(struct transport *trans, const void *buf, unsigned size);
	void (*close)(struct transport *trnas);
};

struct transport {
	const struct transport_ops *ops;
};

static inline int transport_set_baudrate(struct transport *trans, unsigned speed)
{
	if (trans->ops && trans->ops->set_baudrate)
		return trans->ops->set_baudrate(trans, speed);

	errno = -ENOSYS;
	return -1;
}

static inline int transport_write(struct transport *trans, const void *buf, unsigned size)
{
	if (trans->ops && trans->ops->write)
		return trans->ops->write(trans, buf, size);

	errno = -ENOSYS;
	return -1;
}

static inline int transport_read(struct transport *trans, void *buf, unsigned size)
{
	if (trans->ops && trans->ops->read)
		return trans->ops->read(trans, buf, size);

	errno = -ENOSYS;
	return -1;
}

static inline int transport_close(struct transport *trans)
{
	if (trans->ops && trans->ops->close) {
		trans->ops->close(trans);
		return 0;
	}

	errno = -ENOSYS;
	return -1;
}

#define TRANSPORT_IFACE_LIBUSB	"libusb"
#define TRANSPORT_IFACE_HIDAPI	"hidapi"
#define TRANSPORT_IFACE_SERAIL	"serial"

union transport_param {
	struct {
		int iface, flags;
		unsigned short vid, pid;
	} libusb;

	struct {
		unsigned short vid, pid;
	} hidapi;

	struct {
		const char *tty;
		unsigned speed;
	} serial;
};

struct transport *transport_open(const char *transport_name, union transport_param *param);

#endif /* __TRANSPORT_H__*/

