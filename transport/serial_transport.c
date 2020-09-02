/*
 * Written by ZhongYao Luo <luozhongyao@gmail.com>
 *
 * Copyright 2020 ZhongYao Luo
 */

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <limits.h>
#include <termios.h>
#include "transport.h"
#include "baudrate.h"
#include "defs.h"

struct serial_transport {
	int fd;
	struct transport transport;
};

static speed_t tty_get_speed(int speed)
{
#define _(n) case n: return B ##n
	switch (speed) {
		_(9600); break;
		_(19200); break;
		_(38400); break;
		_(57600); break;
		_(115200); break;
		_(230400); break;
		_(460800); break;
		_(500000); break;
		_(576000); break;
		_(921600); break;
		_(1000000); break;
		_(1152000); break;
		_(1500000); break;
		_(2000000); break;
		_(2500000); break;
		_(3000000); break;
		_(3500000); break;
		_(4000000); break;
	default:
		printf("Not support %d\n", speed);
	break;
	}
#undef _
	return -1;
}

static int uart_open(const char *dev, int speed)
{
	int fd;
	speed_t baudrate;
	struct termios ti;
	struct serial_struct serial;

	if (dev == NULL)
		return -1;

	fd = open(dev, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror(dev);
		exit(1);
	}

	if (!isatty(fd))
		return fd;

	tcflush(fd, TCIOFLUSH);

	if (tcgetattr(fd, &ti) < 0) {
		perror("get port settings");
		exit(1);
	}
	cfmakeraw(&ti);
	ti.c_cflag |= CLOCAL;
	ti.c_cflag &= ~CRTSCTS;
	ti.c_cflag &= ~CSTOPB;
	ti.c_cflag |= CS8;
	ti.c_cflag &= ~PARENB;
	ti.c_cc[VMIN] = 1;
	ti.c_cc[VTIME] = 0;
	baudrate = tty_get_speed(speed);
	if (baudrate != -1) {
		cfsetispeed(&ti, baudrate);
		cfsetospeed(&ti, baudrate);
	}

	if (tcsetattr(fd, TCSANOW, &ti) < 0) {
		perror("set port settings");
		exit(1);
	}

	tcflush(fd, TCIOFLUSH);
	if (baudrate == -1) {
		if (set_baudrate(fd, speed)) {
			perror("set baudrate");
			exit(1);
		}
	}

	if (ioctl(fd, TIOCGSERIAL, &serial) == 0) {
		serial.flags |= ASYNC_LOW_LATENCY;
		if (ioctl(fd, TIOCSSERIAL, &serial)) {
			perror("set serial");
		}
	}

	return fd;
}

static int uart_set_baudrate(int fd, unsigned speed)
{
	speed_t baudrate;
	struct termios ti;

	tcgetattr(fd, &ti);
	baudrate = tty_get_speed(speed);
	if (baudrate != -1) {
		cfsetispeed(&ti, baudrate);
		cfsetospeed(&ti, baudrate);
	}

	if (tcsetattr(fd, TCSANOW, &ti) < 0) {
		perror("set port settings");
		return -1;
	}

	tcflush(fd, TCIOFLUSH);
	if (baudrate == -1) {
		if (set_baudrate(fd, speed)) {
			perror("set baudrate");
			return -1;
		}
	}

	return 0;
}

static int serial_write(struct transport *trans, const void *buf, unsigned size)
{
	struct serial_transport *ser = container_of(trans, struct serial_transport, transport);

	return write(ser->fd, buf, size);
}

static int serial_read(struct transport *trans, void *buf, unsigned size)
{
	struct serial_transport *ser = container_of(trans, struct serial_transport, transport);

	return read(ser->fd, buf, size);
}

static void serial_close(struct transport *trans)
{
	struct serial_transport *ser = container_of(trans, struct serial_transport, transport);

	close(ser->fd);
	free(ser);
}

static int serial_set_baudrate(struct transport *trans, unsigned speed)
{
	struct serial_transport *ser = container_of(trans, struct serial_transport, transport);

	return uart_set_baudrate(ser->fd, speed);
}

static const struct transport_ops serial_transport_ops = {
	.write = serial_write,
	.read = serial_read,
	.close = serial_close,
	.set_baudrate = serial_set_baudrate,
};

struct transport *serial_transport_open(const char *dev, unsigned speed)
{
	int fd;
	struct serial_transport *ser;

	fd = uart_open(dev, speed);
	if (fd < 0) {
		return NULL;
	}

	ser = malloc(sizeof(struct serial_transport));

	ser->transport.ops = &serial_transport_ops;
	ser->fd = fd;

	return &ser->transport;
}
