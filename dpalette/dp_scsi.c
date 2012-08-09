#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

#include "dpalette.h"
#include "dp_scsi.h"

static void dp_scsi_decode_inquiry_srb(unsigned char *scb, DPAL_STATE *state)
{
	int v5;

	// Get firmware version from the inquiry data
	state->iFirmwareVersion = atoi((char *)&scb[32]);

	state->field_414 = (scb[40] << 8) | scb[41];
	if (state->iFirmwareVersion == 170)
		state->field_414 -= 62;
	state->field_3CC = 132;		// 132 = 128 + 4

	if (scb[46] == 16)
		v5 = 4;
	else
		v5 = 2;

	if (v5 == 4)
		state->field_3CC |= 0x01;

	state->field_4C4 = (scb[46] << 8) | scb[47];

	if (state->field_414 > 128)
		state->field_3CC |= 2;

	if (scb[36] & 0x01)
		state->field_4D0 = true; //FIXME: TRUE;
	else
		state->field_4D0 = false; //FIXME: FALSE;
}

#if 0
static void dp_scsi_decode_aspi_error(unsigned int aspiStatus, DPAL_STATE *state)
{
	state->iErrorClass = -22;		// FIXME magic number - I think this means "SCSI error"
	state->iErrorNumber = aspiStatus >> 8;

	switch (aspiStatus >> 8) {
		case SS_PENDING:		strcpy(state->sErrorMsg, "SRB being processed"); break;
		case SS_COMP:			strcpy(state->sErrorMsg, "SRB completed without error"); break;
		case SS_ABORTED:		strcpy(state->sErrorMsg, "SRB aborted"); break;
		case SS_ABORT_FAIL:		strcpy(state->sErrorMsg, "Unable to abort SRB"); break;
		case SS_ERR:			strcpy(state->sErrorMsg, "SRB completed with error"); break;
		case SS_INVALID_CMD:	strcpy(state->sErrorMsg, "Invalid ASPI command"); break;
		case SS_INVALID_HA:		strcpy(state->sErrorMsg, "Invalid host adapter number"); break;
		case SS_NO_DEVICE:		strcpy(state->sErrorMsg, "SCSI device not installeds"); break;
		case SS_INVALID_SRB:	strcpy(state->sErrorMsg, "Invalid parameter set in SRB"); break;
#ifndef __WIN32
		case SS_OLD_MANAGER:	strcpy(state->sErrorMsg, "ASPI manager doesn't support Windows"); break;
#else
		case SS_BUFFER_ALIGN:	strcpy(state->sErrorMsg, "Buffer not aligned (replaces OLD_MANAGER in Win32)"); break;
#endif
		case SS_ILLEGAL_MODE:	strcpy(state->sErrorMsg, "Unsupported Windows mode"); break;
		case SS_NO_ASPI:		strcpy(state->sErrorMsg, "No ASPI managers resident"); break;
		case SS_FAILED_INIT:	strcpy(state->sErrorMsg, "ASPI for windows failed init"); break;
		case SS_ASPI_IS_BUSY:	strcpy(state->sErrorMsg, "No resources available to execute cmd"); break;
		case SS_BUFFER_TO_BIG:	strcpy(state->sErrorMsg, "Buffer size to big to handle!"); break;
		case SS_MISMATCHED_COMPONENTS:	strcpy(state->sErrorMsg, "The DLLs/EXEs of ASPI don't version check"); break;
		case SS_NO_ADAPTERS:	strcpy(state->sErrorMsg, "No host adapters to manage"); break;
		case SS_INSUFFICIENT_RESOURCES:	strcpy(state->sErrorMsg, "Couldn't allocate resources needed to init"); break;
		case SS_ASPI_IS_SHUTDOWN:	strcpy(state->sErrorMsg, "Call came to ASPI after PROCESS_DETACH"); break;
		case SS_BAD_INSTALL:	strcpy(state->sErrorMsg, "The DLL or other components are installed wrong"); break;
		default:				sprintf(state->sErrorMsg, "Unknown ASPI error 0x%02X", aspiStatus >> 8);
	}
}
#endif

static char *dp_scsi_decode_sense(DPAL_STATE *state, unsigned char *senseData)
{
	int errCode = senseData[9] | (senseData[8] << 8);

	if (errCode == 11) {
		state->iErrorClass = -1;
		state->iErrorNumber = 11;
		return "Indicates that CFR aborted command";
	}

	// TODO: move this into the case statement
    if ((errCode >= 0x2420) && (errCode <= 0x2428))
    {
		state->iErrorClass = -18;
		state->iErrorNumber = 0x2420;
		return "Calibration error";
    }

	switch (errCode) {
		case 2:
			state->iErrorClass = -22;
			state->iErrorNumber = 2;
			return "Indicates that the LUN addressed can not be accessed";
		case 4:
			state->iErrorClass = -18;
			state->iErrorNumber = 4;
			return "Indicates that CFR firmware detected unrecoverable hardware errors";
		case 5:
			state->iErrorClass = -8;
			state->iErrorNumber = 5;
			return "Indicates that there was an illegal parameter in the command descriptor";
		case 6:
			state->iErrorClass = -17;
			state->iErrorNumber = 6;
			return "Indicates that CFR has been reset";

		case 0x2000:
			state->iErrorClass = -1;
			state->iErrorNumber = 0x2000;
			return "Indicates that no other additional information available";

		case 0x2400:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2400;
			return "Diagnostic failure";
		case 0x2401:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2401;
			return "Memory failure";
		case 0x2402:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2402;
			return "Video Buffer failure";
		case 0x2403:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2403;
			return "Video Data failure";
		case 0x2404:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2404;
			return "General I/O failure";
		case 0x2405:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x2405;
			return "Checksum error";
		case 0x2406:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2406;
			return "Power on failure";
		case 0x2407:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x2407;
			return "Filter wheel jam";
		case 0x2408:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x2408;
			return "Bad filter wheel position";
		case 0x2409:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x2409;
			return "No memory for data queue";
		case 0x240B:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x240B;
			return "Film previously exposed";
		case 0x240C:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x240C;
			return "Bad daughter board configuration";
		case 0x240D:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x240D;
			return "Frame buffer memory failure";
		case 0x240E:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x240E;
			return "Generic firmware error";
		case 0x240F:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x240F;
			return "Unexpected interrrupt";
		case 0x2410:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2410;
			return "Camera fuse blown";
		case 0x2411:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2411;
			return "Unknown camera back";
		case 0x2412:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2412;
			return "Camera film door open";
		case 0x2413:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2413;
			return "Shutter failure";
		case 0x2414:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2414;
			return "Film load failure";
		case 0x2415:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x2415;
			return "Film jammed";
		case 0x2416:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x2416;
			return "Dark slide in place. REMOVE IT.";
		case 0x24FF:
			state->iErrorClass = -18;
			state->iErrorNumber = 0x24FF;
			return "Unknown hardware error";

		case 0x2500:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2500;
			return "Indicates that the LUN addressed can not be accessed";
		case 0x2501:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2501;
			return "Indicates that the LUN addressed can not be accessed";
		case 0x2502:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2502;
			return "Indicates that the LUN addressed can not be accessed";
		case 0x2503:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2503;
			return "Unsupported function";
		case 0x2504:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2504;
			return "Requested length not in valid range";
		case 0x2505:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2505;
			return "Invalid RGB color";
		case 0x2506:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2506;
			return "Invalid combination of FLAG & LINK bit";
		case 0x2507:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2507;
			return "Print command issued without first issuing Start Exposure";
		case 0x2508:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2508;
			return "Print command invalid Transfer Length";
		case 0x2509:
			state->iErrorClass = 0;
			state->iErrorNumber = 0x2509;
			return "Terminate Exposure issued without Start Exposure";
		case 0x250A:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x250A;
			return "Invalid requested length for Set Color Table";
		case 0x250B:
			state->iErrorClass = -24;
			state->iErrorNumber = 0x2509;
			return "Invalid LUN";
		case 0x250C:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x250C;
			return "Statement issued with no termination";
		case 0x250D:
			state->iErrorClass = -24;
			state->iErrorNumber = 0x250D;
			return "Indicates that CFR aborted command";

		case 0x2540:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2540;
			return "Invalid field in the parameter list";
		case 0x2541:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2541;
			return "Unsupported function";
		case 0x2542:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2542;
			return "Requested length not in the range";
		case 0x2543:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2543;
			return "Requested film number incorrect for attached back";
		case 0x2544:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2544;
			return "Requested film number not in range";
		case 0x2545:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2545;
			return "Requested horizontal resolution not in range";
		case 0x2546:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2546;
			return "Requested horizontal offset not in range";
		case 0x2547:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2547;
			return "Requested line length not in range";
		case 0x2548:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2548;
			return "Requested vertical resolution not in range";
		case 0x254A:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x254A;
			return "Requested luminant red not in range";
		case 0x254B:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x254B;
			return "Requested luminant green not in range";
		case 0x254C:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x254C;
			return "Requested luminant blue not in range";
		case 0x254D:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x254D;
			return "Requested color balance red not in range";
		case 0x254E:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x254E;
			return "Requested color balance green not in range";
		case 0x254F:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x254F;
			return "Requested color balance blue not in range";
		case 0x2551:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2551;
			return "Indicates that there was an illegal parameter in the command descriptor";
		case 0x2552:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2552;
			return "Indicates that there was an illegal parameter in the command descriptor";
		case 0x2553:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2553;
			return "Indicates that there was an illegal parameter in the command descriptor";
		case 0x2554:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2554;
			return "Indicates that there was an illegal parameter in the command descriptor";
		case 0x2555:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2555;
			return "Indicates that there was an illegal parameter in the command descriptor";
		case 0x2556:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2556;
			return "Requested image enhancement not in range";
		case 0x2557:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2557;
			return "Camera adjust command invalid parameters";
		case 0x2558:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2558;
			return "Bad print command line number";
		case 0x2559:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2559;
			return "Calibration control byte not in range";
		case 0x255A:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x255A;
			return "Image queue byte not in range";
		case 0x255B:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x255B;
			return "Downloaded filmtable, bad data in table";
		case 0x255C:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x255C;
			return "Downloaded filmtable bad table size";
		case 0x255D:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x255D;
			return "Background color command, bad parameter";
		case 0x255E:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x255E;
			return "Image brightness out of range";
		case 0x255F:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x255F;
			return "Invalid servo mode";
		case 0x2560:
		case 0x2561:
		case 0x2562:
		case 0x2563:
		case 0x2564:
		case 0x2565:
		case 0x2566:
		case 0x2567:
		case 0x2568:
		case 0x2569:
		case 0x256A:
		case 0x256B:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2561;
			return "Frame buffer system error";
		case 0x256C:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x256C;
			return "Requested min exposure resolution not in range";
		case 0x256D:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x256D;
			return "Start exposure issued with the single mode exposure in process";
		case 0x256E:
		case 0x256F:
		case 0x2570:
		case 0x2571:
		case 0x2572:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x256E;
			return "Frame buffer system error";
		case 0x2573:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2573;
			return "Invalid data in set exposure fix parameters command";
		case 0x2574:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2574;
			return "Invalid data in get failures command";
		case 0x2575:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2575;
			return "Film type does not match locked film";
		case 0x2576:
			state->iErrorClass = -2;
			state->iErrorNumber = 0x2576;
			return "Flash writer error";

		case 0x2580:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2580;
			return "Film table has bad camera data";
		case 0x2581:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2581;
			return "Wrong number of pixel tables in the film table";
		case 0x2582:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2582;
			return "First pixel table is missing in film table";
		case 0x2583:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2583;
			return "Pixel tables are out of order in the film table";
		case 0x2584:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2584;
			return "Vertical doubles error in film table";
		case 0x2585:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2585;
			return "Scans error in the film table";
		case 0x2586:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2586;
			return "4096 pixel table is missing in the film table";
		case 0x2587:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2587;
			return "4097 pixel table is missing in the film table";
		case 0x2588:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x2588;
			return "8192 pixel table is missing in the film table";

		case 0x258F:
			state->iErrorClass = -8;
			state->iErrorNumber = 0x258F;
			return "Unknown film table data error";

		case 0x2600:
			state->iErrorClass = 0;
			state->iErrorNumber = 0x2600;
			return "Power-on reset or bus devoice reset occurred";

		case 0x2B00:
			state->iErrorClass = -1;
			state->iErrorNumber = 0x2B00;
			return "Unsuccessful message retry";
		case 0x2B01:
			state->iErrorClass = -1;
			state->iErrorNumber = 0x2B01;
			return "Message parity error";
		case 0x2B02:
			state->iErrorClass = -1;
			state->iErrorNumber = 0x2B02;
			return "Interface parity error";

		case 0x4000:
			state->iErrorClass = -17;
			state->iErrorNumber = 0x4000;
			return "Software error in the Digital Palette bus occurred";

		default:
			// LABEL_99:
			state->iErrorClass = -1;
			state->iErrorNumber = -2;
			return "Unknown firmware error";
	}
}

static int dp_scsi_do_request_sense(DPAL_STATE *state, unsigned char *senseData)
{
	unsigned char senseBuf[10];

	DP_doscsi_cmd(state, 0x03, senseBuf, sizeof(senseBuf), 0, 0, 0);

	if ((senseBuf[8] != 0x20) || (senseBuf[9] != 0)) {
		state->iErrorClass = -2;
		state->iErrorNumber = senseData[8] << 8 | senseData[9];
		strcpy(state->sErrorMsg, "Can not clear error occured");
		return state->iErrorClass;
	}

	if (senseData[2] & 0x20) {
		state->iErrorClass = -1;
		state->iErrorNumber = -1;
		strcpy(state->sErrorMsg, "Requested logical block length does not match the physical block length");
		return state->iErrorClass;
	}

	if (senseData[2] & 0x40) {
		state->iErrorClass = 3;
		state->iErrorNumber = 3;
		strcpy(state->sErrorMsg, "Out of film");
		return state->iErrorClass;
	}

	if (senseData[2] & 0x80) {
		state->iErrorClass = 0;
		state->iErrorNumber = 0;
		strcpy(state->sErrorMsg, "Exposure in progress");
		return state->iErrorClass;
	}

	if (senseData[0] & 0x70) {
		strcpy(state->sErrorMsg, dp_scsi_decode_sense(state, senseData));
		return state->iErrorClass;
	}

	if ((senseData[2] & 0xF0) && (senseData[0] & 0x7E)) {
		state->iErrorClass = -1;
		state->iErrorNumber = (senseData[8] << 8) | senseData[9];
		strcpy(state->sErrorMsg, dp_scsi_decode_sense(state, senseData));
		return state->iErrorClass;
	}

	return 0;
}

int DP_scsi_init(DPAL_STATE *state, char *filmRecorderID)
{
	unsigned char buf[99];

	state->iDeviceFd = open(filmRecorderID, O_RDWR);
	if (state->iDeviceFd < 0) {
		state->iErrorClass = -18;
		state->iErrorNumber = 0x2702;
		strcpy(state->sErrorMsg, "Failed to open sg device node");
		return state->iErrorClass;
	}

	// FIXME debug
	printf("SCSI FD = %d\n", state->iDeviceFd);

	if (DP_doscsi_cmd(state, 0x12, buf, sizeof(buf), 0, 0, 0)) {
		state->iErrorClass = -18;
		state->iErrorNumber = -4;
		strcpy(state->sErrorMsg, "Digital Palette is Busy, Disconnected, OFF or Not Initialized");
		return state->iErrorClass;
	}

	if ((strncmp((char *)&buf[8], "DP2SCSI", strlen("DP2SCSI")) == 0) &&
		(strncmp((char *)&buf[16], "Polaroid", strlen("Polaroid")) == 0)) {
		dp_scsi_decode_inquiry_srb(buf, state);
		return 0;
	} else {
		state->iErrorClass = -18;
		state->iErrorNumber = -4;
		strcpy(state->sErrorMsg, "Digital Palette is Busy, Disconnected, OFF or Not Initialized");
		return state->iErrorClass;
	}
}

int DP_doscsi_cmd(DPAL_STATE *state, int scsiCmd, void *SRB_Buffer, size_t szSRB_Buffer, int scsiPageCode, char vendorControlBits, DP_SCSI_DIRECTION dir)
{
	sg_io_hdr_t io_hdr;
	unsigned char cdb[6];
	unsigned char senseBuf[14];

	// Prepare the SCSI CDB
	cdb[0] = scsiCmd;
	cdb[1] = 0;
	cdb[2] = scsiPageCode;
	cdb[3] = (szSRB_Buffer >> 8);
	cdb[4] = szSRB_Buffer & 0xff;
	cdb[5] = vendorControlBits << 6;

	// Prepare the SCSI I/O header
	memset(&io_hdr, '\0', sizeof(sg_io_hdr_t));
	io_hdr.interface_id = 'S';	// SCSI
	io_hdr.iovec_count = 0;		// memset does this for us but it never hurts to be sure :)

	// data transfer direction
	if (dir == SCSI_DIR_INPUT) {
		io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	} else if (dir == SCSI_DIR_OUTPUT) {
		io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	} else if (dir == SCSI_DIR_NONE) {
		io_hdr.dxfer_direction = SG_DXFER_NONE;
	}

	// data buffer
	io_hdr.dxferp = SRB_Buffer;

	// Commands 0C:04 (Read Film Name) and 0C:05 (Read Aspect Ratio) accept a
	// film ID number in the most significant byte of szSRB_Buffer.
	// This code ANDs off the film number to get the transfer length.
	if ((scsiCmd != 0x0C) || ((scsiPageCode != 0x04) && (scsiPageCode != 0x05)))
		io_hdr.dxfer_len = szSRB_Buffer;
	else
		io_hdr.dxfer_len = szSRB_Buffer & 0xFF;

	// command buffer
	io_hdr.cmdp = cdb;
	io_hdr.cmd_len = sizeof(cdb);

	// sense buffer
	memset(senseBuf, '\0', sizeof(senseBuf));
	io_hdr.sbp = senseBuf;
	io_hdr.mx_sb_len = sizeof(senseBuf);

	// options
	io_hdr.timeout = 20000;		// 20,000 milliseconds = 20 seconds
	io_hdr.flags = 0;			// defaults: indirect I/O, etc.
	io_hdr.pack_id = 0;
	io_hdr.usr_ptr = NULL;

	if (ioctl(state->iDeviceFd, SG_IO, &io_hdr) < 0) {
		// FIXME this needs the error code checking
		perror("SG_IO error");
		return -1;
	}

	// FIXME if CHECK CONDITION then do a Request Sense and decode the sense buffer
/*
	if (srb->SRB_TargStat == 0x02) {		// STATUS_CHKCOND
		return dp_scsi_do_request_sense(state, srb->SenseArea);
	} else {
		return 0;
	}
*/
	return 0;
}

int DP_scsi_inq(DPAL_STATE *state)
{
	int err;
	unsigned char srbBuffer[63];

	err = DP_doscsi_cmd(state, 0x12, srbBuffer, sizeof(srbBuffer), 0, 0, 0);
	if (err)
		return err;

	dp_scsi_decode_inquiry_srb(srbBuffer, state);
	state->iErrorClass = 0;
	return state->iErrorClass;
}
