#include <iostream>
#include <string>

#include <string.h>	// memset
#include <stdio.h>
#include <ctype.h>

/*
#include <sys/types.h>
#include <sys/stat.h>
*/
#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

#include "hexdump.h"

using namespace std;

/////////////////////////

void usage(char *progname)
{
	cerr << "Usage:\n";
	cerr << "\t" << progname << " pnmfile\n";
}

/////////////////////////

string scsi_device = "/dev/dp2scsi";

int main(int argc, char **argv)
{
	// FIXME: argc will always be at least 1 (due to the program name being argv[0])
	if (argc < 1) {
		usage(argv[0]);
		return 1;
	}

	int sg_fd = open(scsi_device.c_str(), O_RDWR);
	if (sg_fd < 0) {
		cerr << "Error opening SCSI device\n";
		return 1;
	}
	cout << "scsi_fd = " << sg_fd << endl;

	// Send a SCSI INQUIRY command
	unsigned char cdb[6], buf[96], sense_buffer[32];
	memset(cdb, '\0', sizeof(cdb));
	cdb[0] = 0x12;
	cdb[4] = (sizeof(buf) > 255) ? 255 : sizeof(buf);

	sg_io_hdr_t io_hdr;
	memset(&io_hdr, '\0', sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';	// SCSI
	io_hdr.cmd_len = sizeof(cdb);
	io_hdr.iovec_count = 0;		// memset does this for us but it never hurts to be sure :)
	io_hdr.mx_sb_len = sizeof(sense_buffer);
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	io_hdr.dxfer_len = sizeof(buf);
	io_hdr.dxferp = &buf;
	io_hdr.cmdp = cdb;
	io_hdr.sbp = sense_buffer;
	io_hdr.timeout = 20000;		// 20,000 milliseconds = 20 seconds
	io_hdr.flags = 0;			// defaults: indirect I/O, etc.
	io_hdr.pack_id = 0;
	io_hdr.usr_ptr = NULL;

	if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
		perror("SG_IO error");
		return -1;
	}

	if ((io_hdr.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
		// tum te tum - print SENSE data here
		printf("SCSI failed\n");
	} else {
		// success
		hex_dump(buf, buf[4]+5);
		printf("Inquiry duration=%u millisecs, resid=%d\n", io_hdr.duration, io_hdr.resid);
	}

	close(sg_fd);

	return 0;
}
