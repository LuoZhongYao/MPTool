#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define OGF_VENDOR_CMD					0x3f
#define HCI_VENDOR_CHANGE_BAUD			0x17
#define HCI_VENDOR_DOWNLOAD				0x20
#define HCI_VENDOR_READ_CHIP_TYPE		0x61
#define HCI_VENDOR_x62					0x62
#define HCI_VENDOR_READ_ROM_VER			0x6d
#define HCI_VENDOR_READ_EVERSION		0x6f
#define HCI_VENDOR_WRITE_FREQ_OFFSET	0xea

#define cmd_opcode_pack(ogf, ocf)	(uint16_t)((ocf & 0x03ff)|(ogf << 10))
#define cmd_opcode_ogf(op)		(op >> 10)
#define cmd_opcode_ocf(op)		(op & 0x03ff)

extern void hci_send_cmd(uint16_t opcode, const void *params, uint8_t size);
extern int hci_send_cmd_sync(uint16_t opcode, const void *params, uint8_t size,
	void *rsp, uint16_t rsp_size);

static uint32_t rtlbt_baudrate(uint32_t baudrate)
{
#define _(b, r)  case b: return r
	switch (baudrate) {
		_(115200,  0x0252C014);
		_(230400,  0x0252C00A);
		_(921600,  0x05F75004);
		_(1000000, 0x0252c014);
		_(1500000, 0x04928002);
		_(2000000, 0x00005002);
		_(2500000, 0x0000B001);
		_(3000000, 0x04928001);
		_(3500000, 0x052A6001);
		_(4000000, 0x00005001);
	}
#undef _
	return 0x0252C014;
}

int rtlbt_read_chip_type(void)
{
	uint8_t rsp[5];
	const uint8_t params[5] = {0x20, 0xa8, 0x02, 0x00, 0x40};
	return hci_send_cmd_sync(cmd_opcode_pack(OGF_VENDOR_CMD, HCI_VENDOR_READ_CHIP_TYPE),
		params, sizeof(params), rsp, 5);
}

int rtlbt_vendor_cmd62(const unsigned char dat[9])
{
	int rs;
	uint8_t status;

	rs = hci_send_cmd_sync(cmd_opcode_pack(OGF_VENDOR_CMD, HCI_VENDOR_x62),
		dat, 9, &status, 1);

	return rs ? rs : status;
}

int rtlbt_change_baudrate(unsigned baudrate)
{
	int rs;
	uint8_t status;
	uint32_t rtlbaudrate;

	rtlbaudrate = rtlbt_baudrate(baudrate);
	rs = hci_send_cmd_sync(cmd_opcode_pack(OGF_VENDOR_CMD, HCI_VENDOR_CHANGE_BAUD),
		&rtlbaudrate, sizeof(rtlbaudrate), &status, 1);

	return rs ? rs : status;
}

int rtlbt_fw_download(const char *fw)
{
	int fd;
	bool cmpl;
	uint8_t rsp[2];
	uint8_t buf[256];
	uint8_t rz;
	uint8_t off = 0;
	uint32_t count = 0;
	uint16_t opcode = cmd_opcode_pack(OGF_VENDOR_CMD, HCI_VENDOR_DOWNLOAD);
	struct stat stbuf;
	int rs = -1;

	if (-1 == stat(fw, &stbuf)) {
		return -1;
	}


	fd = open(fw, O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	off = 0;
	count = 0;
	cmpl = false;
	do {
		rz = read(fd, buf + 1, 252);
		buf[0] = off & 0x7f;
		count += rz;
		if (count == stbuf.st_size) {
			cmpl = true;
			//buf[0] |= 0x80;
		}

		hci_send_cmd_sync(opcode, buf, rz + 1, rsp, 2);
		if (rsp[0] != 0 || rsp[1] != (off & 0x7f)) {
			errno = EIO;
			goto __quit;
		}
		off++;
	} while (!cmpl);

	rs = 0;
__quit:
	close(fd);
	return rs;
}
