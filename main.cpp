#include <iostream>
#include <string>

#include <stdlib.h>
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
#include "dpalette/dpalette.h"

using namespace std;

/////////////////////////

void usage(char *progname)
{
	cerr << "Usage:\n";
	cerr << "\t" << progname << " pnmfile\n";
}

/////////////////////////

string scsi_device = "/dev/dp2scsi";

DPAL_STATE state;

void DPCHECK(int i)
{
	if (i != 0) {
		printf("DPAL ERROR %d\niErrorClass %d, iErrorNumber %d [%s]\n",
				i, state.iErrorClass, state.iErrorNumber, state.sErrorMsg);
		exit(-1);
	}
}

int main(int argc, char **argv)
{
#if 0
	// FIXME: argc will always be at least 1 (due to the program name being argv[0])
	if (argc < 1) {
		usage(argv[0]);
		return 1;
	}
#endif

	DPCHECK(DP_InitPrinter(&state, true, "/dev/dp2scsi"));

	printf("Statedump!\n");
	hex_dump(&state, sizeof(state));

	return 0;
}
