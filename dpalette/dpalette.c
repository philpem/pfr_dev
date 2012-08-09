#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <dirent.h>

#include "dpalette.h"
#include "dp_scsi.h"
#include "dp_filmtable_crypt.h"
#include "dp_log.h"

typedef struct {
	char filmFile[PATH_MAX];
	char filmName[32];			// FIXME actually only 24 chars + null term
} FILMTABLE;

bool _global_libraryIsInitialised = false;
int _global_numKnownFilmTypes;
FILMTABLE *_global_films = NULL;

int _global_logLevel = 0;
char _global_logFilename[15];
char _global_filmProfilePath[120];
char _global_byte10014288;

/// --- MACROS ---
// These delay macros replace the horribly inefficient functions in Polaroid's
// original driver library with more efficient equivalents.
#define delaySeconds(x)			sleep(x)
#define delayMilliseconds(x)	usleep(x * 1000)
/// --- END MACROS ---


/// --- STATIC FUNCTIONS ---
static int dp_open_film_table(DPAL_STATE *state, const char *filename)
{
	state->FFilmTable = fopen(filename, "rb");
	if (state->FFilmTable)
		return 0;
	else
		return -1;
}

static int dp_read_filmtable_bytes(DPAL_STATE *state, unsigned char *dstBuf, size_t count)
{
	return fread(dstBuf, 1, count, state->FFilmTable);
}

static void dp_close_filmtable(DPAL_STATE *state)
{
	fclose(state->FFilmTable);
}

static void dp_PrepareParamBuffer(DPAL_STATE *state, unsigned char *buf)
{
	size_t pos;
	buf[0] = buf[1] = buf[2] = 0;
	pos=3;
	buf[pos++] = 39;
	buf[pos++] = _global_byte10014288;
	buf[pos++] = 0;
	buf[pos++] = (state->iHorRes >> 8) & 0xff;
	buf[pos++] = state->iHorRes & 0xff;
	buf[pos++] = (state->iHorOff >> 8) & 0xff;
	buf[pos++] = state->iHorOff & 0xff;
	buf[pos++] = (state->iLineLength >> 8) & 0xff;
	buf[pos++] = state->iLineLength & 0xff;
	buf[pos++] = 0;
	buf[pos++] = (state->iVerRes >> 8) & 0xff;
	buf[pos++] = state->iVerRes & 0xff;
	buf[pos++] = (state->iVerOff >> 8) & 0xff;
	buf[pos++] = state->iVerOff & 0xff;
	buf[pos++] = 0;
	buf[pos++] = state->ucaLuminantRed;
	buf[pos++] = state->ucaLuminantGreen;
	buf[pos++] = state->ucaLuminantBlue;
	buf[pos++] = 0;
	buf[pos++] = state->caColorBalRed;
	buf[pos++] = state->caColorBalGreen;
	buf[pos++] = state->caColorBalBlue;
	buf[pos++] = 0;
	buf[pos++] = state->ucaExposTimeRed;
	buf[pos++] = state->ucaExposTimeGreen;
	buf[pos++] = state->ucaExposTimeBlue;
	buf[pos++] = 0;
	buf[pos++] = state->cLiteDark;
	buf[pos++] = (state->UNK_numTotalLines >> 8) & 0xff;
	buf[pos++] = state->UNK_numTotalLines & 0xff;
	buf[pos++] = state->cServo;

	// FIXME? No idea if this is correct.
	if (state->cImageCompression == 0) {
		buf[34] = 1;
	} else {
		if (state->cImageCompression != 1) {
			if (state->cImageCompression != 2) {
				buf[34] = state->cImageCompression;
			} else {
				buf[34] = 1;
			}
		} else {
			buf[34] = 0;
		}
	}
	pos++;

	buf[pos++] = state->field_30[2];
	buf[pos++] = state->field_30[3];
	buf[pos++] = state->field_30[0];
	buf[pos++] = 0;
	buf[pos++] = state->field_30[1];

	buf[pos++] = ((state->field_4C0 >> 8) & 0xff) | 0x80;
	buf[pos++] = (state->field_4C0) & 0xff;
	buf[pos++] = 0;
}

// read a bigendian int from an array
static unsigned int dp_read_int_be(const unsigned char *buf, const size_t offset)
{
	return (((unsigned int) buf[offset] << 8) & 0xFF00) | (buf[offset + 1] & 0xFF);
}

// write a bigendian int into an array
static void dp_write_int_be(const unsigned int val, unsigned char *buf, const size_t offset)
{
	buf[offset]   = (val >> 8) & 0xff;
	buf[offset+1] = val & 0xff;
}

static bool waitAndUpdateBufferFree(DPAL_STATE *state, const size_t nbytes)
{
	int nBufElems;

	nBufElems = ((nbytes + 100) / 1024) + 1;

	if (3 * state->iBufferFree / 4 <= (((nbytes + 100) / 1024) + 5)) {
		do {
			if (DP_GetPrinterStatus(state, 64))		// FIXME: MAGIC NUMBERS
				return state->iErrorClass;

			if (state->iBufferFree < nBufElems) {
				if (!state->bBufferWait) {
					state->iErrorClass = 1;
					return state->iErrorClass;
				}
				delaySeconds(6);
			}
		} while (state->iBufferFree < nBufElems);

		return state->iErrorClass;
	} else {
		// buffer space available; update
		state->iBufferFree -= nBufElems;
		return 0;
	}
}

static int GetFilmTablePath(DP_MODEL model, char *buf, size_t bufSz)
{
	char *FilmRecorderModels[] = { "CI5000", "HR6000", "PP7000", "PP8000" };

	// Range check
	if ((model < 0) || (model > (sizeof(FilmRecorderModels)/sizeof(FilmRecorderModels[0]))))
		return 1;

#if defined(__WIN32__)
	DWORD cbData;
	HKEY hKey;

	cbData = bufSz;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Polaroid\\Digital Palette\\Paths", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
		*buf = '\0';
		return 1;
	} else {
		if (RegQueryValueEx(hKey, FilmRecorderModels[model], 0, 0, (LPBYTE)buf, &cbData) != ERROR_SUCCESS) {
			// failure
			RegCloseKey(hKey);
			*buf = '\0';
			return 1;
		} else {
			// success
			RegCloseKey(hKey);
			return 0;
		}
	}
#elif defined(__linux)
	// FIXME hard coded path
	snprintf(buf, bufSz, "%s/%s", "/opt/dpalette/film", FilmRecorderModels[model]);
	return 0;
#endif
}

// FIXME: better name plskthx?
static void sub_10004D2B(DPAL_STATE *state)
{
	if (state->iErrorClass) {
		if (state->iErrorNumber == 1)
			strcpy(state->sErrorMsg, "Illegal Image Compression.");
		else
			if (state->iErrorNumber == 2)
				strcpy(state->sErrorMsg, "Illegal Exposure Termination Method.");
			else
				strcpy(state->sErrorMsg, "Incorrect Toolkit Usage.");
	} else {
		state->iErrorClass = -7;
		state->iErrorNumber = 6969;
		strcpy(state->sErrorMsg, "Incorrect Toolkit Usage.");
	}
}

/// --- END STATIC FUNCTIONS ---

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


int DP_StartExposure(DPAL_STATE *state)
{
	if (_global_logLevel && LOG_Debug(state, __func__, DBG_STATUS))
		return state->iErrorClass;

	if (DP_ExposureWarning(state)) {
		if (_global_logLevel)
			LOG_Error(state, __func__, ERR_STATUS);
		return DP_GetPrinterStatus(state, 1);			// FIXME: MAGIC NUMBER
	}

	state->iErrorClass = DP_doscsi_cmd(state, 0x0C, 0, 0, 0, 0, 3);		// FIXME: MAGIC NUMBERS

	if (state->iErrorClass) {
		if (_global_logLevel)
			LOG_Error(state, __func__, ERR_STATUS);
		return state->iErrorClass;
	}

	return state->iErrorClass;
}

int DP_SendPrinterParams(DPAL_STATE *state)
{
	unsigned char paramBuf[43];
	unsigned char camAdjBuf[4];
	unsigned char extBuf[10];
	int i;

	if (_global_logLevel && LOG_Debug(state, __func__, DBG_STATUS))
		return state->iErrorClass;

	dp_PrepareParamBuffer(state, paramBuf);

	state->iErrorClass = DP_doscsi_cmd(state, 0x15, paramBuf, sizeof(paramBuf), 0, 0, 1);	// FIXME: MAGIC NUMBERS

	if (state->iErrorClass) {
		if (_global_logLevel) {
			LOG_Error(state, __func__, ERR_STATUS);
		}
		return DP_GetPrinterStatus(state, 1);		// FIXME: MAGIC NUMBERS
	}

	for (i=0; i<2; i++) {
		state->iErrorClass = DP_doscsi_cmd(state,						// FIXME: MAGIC NUMBERS
											0x0C,
											&state->ColourLUT[256*i],
											256,
											1,
											i,
											1);
		if (state->iErrorClass) {
			if (_global_logLevel) {
				LOG_Error(state, __func__, ERR_STATUS);
			}
			return DP_GetPrinterStatus(state, 1);
		}
	}

	state->iErrorClass = DP_doscsi_cmd(state, 0x0C, state->field_75, sizeof(state->field_75), 8, 0, 1);		// FIXME: MAGIC NUMBERS

	if (state->iErrorClass) {
		if (_global_logLevel) {
			LOG_Error(state, __func__, ERR_STATUS);
		}
		return DP_GetPrinterStatus(state, 1);
	}

	camAdjBuf[0] = _global_byte10014288;
	camAdjBuf[1] = state->caCamAdjustX;
	camAdjBuf[2] = state->caCamAdjustY;
	camAdjBuf[3] = state->caCamAdjustZ;

	state->iErrorClass = DP_doscsi_cmd(state, 0x0C, camAdjBuf, sizeof(camAdjBuf), 9, 0, 1);		// FIXME: MAGIC NUMBERS

	if (state->iErrorClass) {
		if (_global_logLevel) {
			LOG_Error(state, __func__, ERR_STATUS);
		}
		return DP_GetPrinterStatus(state, 1);
	}

	if (state->iFirmwareVersion > 304) {
		dp_write_int_be(state->field_4D4[1], extBuf, 0);
		dp_write_int_be(state->field_4D8[0],  extBuf, 2);
		dp_write_int_be(state->field_4D8[1],  extBuf, 4);
		dp_write_int_be(state->field_4D8[2],  extBuf, 6);
		dp_write_int_be(state->field_4D4[0], extBuf, 8);

		state->iErrorClass = DP_doscsi_cmd(state, 0x0C, extBuf, sizeof(extBuf), 0x12, 0, 1);		// FIXME: MAGIC NUMBERS

		// FIXME? I think this is right, but it may be in the wrong place...
		if (state->iErrorClass) {
			if (_global_logLevel) {
				LOG_Error(state, __func__, ERR_STATUS);
			}
			return DP_GetPrinterStatus(state, 1);
		}
	}

	state->iErrorClass = DP_doscsi_cmd(state, 0x0C, state->field_75, sizeof(state->field_75), 8, 0, 1);		// FIXME: MAGIC NUMBERS

	// FIXME? I think this is right, but it may be in the wrong place...
	if (state->iErrorClass) {
		if (_global_logLevel) {
			LOG_Error(state, __func__, ERR_STATUS);
		}
		return DP_GetPrinterStatus(state, 1);
	}

	return state->iErrorClass;
}

// NOTE: 'flags' is probably the colour wheel number
int DP_SendImageData(DPAL_STATE *state, const unsigned int lineNumber, const unsigned char *src, const size_t size, unsigned char flags)
{
	static unsigned char imagedata_buffer[8192];	// max 8K resolution

	if (_global_logLevel == 1 && LOG_Debug(state, __func__, DBG_MESSAGE))
		return state->iErrorClass;

	if (_global_logLevel == 2 && LOG_Debug(state, __func__, DBG_STATUS))
		return state->iErrorClass;

	if (lineNumber || !DP_GetPrinterStatus(state, 64)) {		// FIXME: MAGIC NUMBERS
		while (waitAndUpdateBufferFree(state, size+5))
			delayMilliseconds(300);

		if (state->iErrorClass) {
			if (_global_logLevel == 1)
				LOG_Error(state, __func__, ERR_MESSAGE);
			if (_global_logLevel == 2)
				LOG_Error(state, __func__, ERR_STATUS);
			return state->iErrorClass;
		}

		if (state->cImageCompression)
			memcpy(&imagedata_buffer[2], src, size);
		dp_write_int_be(lineNumber, imagedata_buffer, 0);
		state->iErrorClass = DP_doscsi_cmd(state, 0x0A, imagedata_buffer, size + 2, 0, flags, 1);	// FIXME: MAGIC NUMBERS

		if (state->iErrorClass) {
			if (_global_logLevel == 1)
				LOG_Error(state, __func__, ERR_MESSAGE);
			if (_global_logLevel == 2)
				LOG_Error(state, __func__, ERR_STATUS);
			return state->iErrorClass;
		}
	}

	return state->iErrorClass;
}

int DP_GetPrinterStatus(DPAL_STATE *state, int mode)
{
#define UPDATE_ERROR_CLASS(state)			\
	/* Somehow I think this code block is wrong. */									\
	if (state->iErrorClass > 9999) {			/* 10,000 + */						\
		if (state->iErrorClass < 10020) {		/* 10,000 to 10,019 */				\
			state->iErrorNumber = state->iErrorClass;								\
			state->iErrorClass = 2;													\
			strcpy(state->sErrorMsg, "No SCSI card detected or ASPI manager not installed");			\
		}																			\
		if (state->iErrorClass > 10012)	{		/* 10,020 to {inf} but >= 10049 */	\
			state->iErrorNumber = state->iErrorClass;								\
			state->iErrorClass = 4;													\
			strcpy(state->sErrorMsg, "Digital Palette is Busy, Disconnected, OFF or Not Initialized");	\
		}																			\
		if (state->iErrorClass < 10049) {		/* ??? */							\
			state->iErrorNumber = state->iErrorClass;								\
			state->iErrorClass = 5;													\
			strcpy(state->sErrorMsg, "SCSI DLL error - Initialization, Memory Buffer, Command, etc");	\
		}																			\
	}

	unsigned char buf[64];

	if (_global_logLevel && LOG_Debug(state, __func__, DBG_STATUS))
		return state->iErrorClass;

	// mode & 0x01 --> query DPalette basic status?
	if (mode & 0x01) {		// FIXME: MAGIC NUMBER
		state->iErrorClass = DP_doscsi_cmd(state, 0, NULL, 0, 0, 0, SCSI_DIR_NONE);
		UPDATE_ERROR_CLASS(state);

		switch (state->iErrorClass) {
			case -3:
				state->field_471 = 0;
				state->field_470 = 0;
				return state->iErrorClass;
			case 1:
				strcpy(state->sErrorMsg, "Digital Palette Buffer is Full");
				return state->iErrorClass;
			case 2:
				strcpy(state->sErrorMsg, "Digital Palette Exposure in progress");
				return state->iErrorClass;
			case 3:
				strcpy(state->sErrorMsg, "Digital Palette Exposure Complete. REMOVE FILM!");
				return state->iErrorClass;
			case -7:
				sub_10004D2B(state);
				return state->iErrorClass;
			case 0:
				state->iErrorNumber = 9;
				strcpy(state->sErrorMsg, "Digital Palette reports no errors");
				break;
			default:	// FIXME probably not necessary
				break;
		}
	}

	// 0x08 means "decode inquiry response"
	// 0x40 does a SCSI Inquiry then decodes the response
	// 0x48 does the same as 0x40
	//
	// updates: status bytes 0 and 1, exposed lines, buffer free, exposure status string
	if (mode & 0x48) {		// FIXME: MAGIC NUMBER
		// 0x40: Do Inquiry
		if (mode & 0x40) {		// FIXME: MAGIC NUMBER
			state->iErrorClass = DP_scsi_inq(state);
			UPDATE_ERROR_CLASS(state);

			if (state->iErrorClass)
				return state->iErrorClass;
		}

		state->iErrorClass = DP_doscsi_cmd(state, 0x0C, buf, 7, 6, 0, 0);		// FIXME: magic numbers

		UPDATE_ERROR_CLASS(state);
		if (state->iErrorClass)
			return state->iErrorClass;

		state->ucStatusByte0 = 0x80;
		if (buf[0] || buf[1])					// b0 -> buffer has free space
			state->ucStatusByte0 |= 0x01;
		if ((buf[5] < 3) || buf[4])				// FIXME: original code casts buf[5] to signed int first
			state->ucStatusByte0 |= 0x02;		// b1 -> based on lower code, "currently exposing"?
		if (buf[6] == 1)
			state->ucStatusByte0 |= 0x04;		// FIXME: not a clue what this means

		// FIXME? (ucStatusByte1) might not have the precedence rules right here
		// What we end up with is the lowest 5 bits coming from buf[4], then 2 more bits from buf[5] and MSB set
		state->ucStatusByte1 = (buf[4] & 0x1F) | ((buf[5] << 5) & 0x60) | 0x80;
		state->uiExposedLines = (buf[2] << 8) | buf[3];
		state->iBufferFree = (buf[0] << 8) | buf[1];

		if (state->ucStatusByte0 & 0x02) {
			// currently exposing?
			const char *ColourWheelStrs[] = { "Red", "Green", "Blue" };
			int curColourWheel = (state->ucStatusByte1 & 0x60) >> 5;

			// curColourWheel:
			//   0=red, 1=green, 2=blue, 3=greyscale

			if ((curColourWheel <= -1) || (curColourWheel >= 2)) {
				sprintf(state->saExposureStatus, "%s, %d lines", "Grayscale", state->uiExposedLines);
			} else {
				sprintf(state->saExposureStatus, "%s, %d lines", ColourWheelStrs[curColourWheel], state->uiExposedLines);
			}
		} else {
			state->saExposureStatus[0] = '\0';
		}
	}

	// mode & 0x02 --> get camera type
	if (mode & 0x02) {		// FIXME: MAGIC NUMBER
		state->iErrorClass = DP_doscsi_cmd(state, 0x1A, buf, 61, 0, 0, 0);	// FIXME: MAGIC NUMBER   scsi read mode page
		UPDATE_ERROR_CLASS(state);

		if (state->iErrorClass)
			return state->iErrorClass;

		state->ucCameraCode = buf[45] | 0x80;
		strcpy(state->saCameraType, (char *)&buf[46]);
	}

	// mode & 0x10 --> get film name
	if (mode & 0x10) {		// FIXME: MAGIC NUMBER
		state->iErrorClass = DP_doscsi_cmd(state, 0x0C, (unsigned char *)&state->saFilmName, (_global_byte10014288 << 8) | 24, 4, 0, 0); // 24 = 0x18 FIXME length of str?
				// FIXME: MAGIC NUMBER ^^^

		if (state->iErrorClass) {
			if ((state->iErrorClass != -1) || (state->iErrorNumber != 9540)) {
				DP_GetPrinterStatus(state, 1);	// FIXME: MAGIC NUMBER
				return state->iErrorClass;
			}
			state->saFilmName[0] = 0;
			state->iErrorClass = 0;
		}
		DP_GetPrinterStatus(state, 1);	// FIXME: MAGIC NUMBER
	}

	// mode & 0x20 --> get aspect ratio
	if (mode & 0x20) {		// FIXME: MAGIC NUMBER
		if (state->saFilmName[0]) {
			state->iErrorClass = DP_doscsi_cmd(state, 0x0C, state->caAspectRatio, (_global_byte10014288 << 8) | 2, 5, 0, 0);	// FIXME 2 = length?
					// FIXME: MAGIC NUMBER ^^^

			if (state->iErrorClass)
				return DP_GetPrinterStatus(state, 1);
		} else {
			state->caAspectRatio[0] = 0;
			state->caAspectRatio[1] = 0;
		}
		state->field_40F = 0;
	}

	// mode & 0x8000 -->
	if (mode & 0x8000) {	// FIXME: MAGIC NUMBER
		if (state->iFirmwareVersion > 304) {
			state->iErrorClass = DP_doscsi_cmd(state, 0x0C, buf, 4, 20, 0, 0);	// FOXME: MAGIC NUMBERS

			if (state->iErrorClass)
				return DP_GetPrinterStatus(state, 1);

			state->field_4C8[0] = dp_read_int_be(buf, 0);
			state->field_4C8[1] = dp_read_int_be(buf, 2);
		} else {
			state->field_4C8[0] = 0;
			state->field_4C8[1] = 0;
		}
	}

	return state->iErrorClass;

#undef UPDATE_ERROR_CLASS
}

int DP_firmware_rev(DPAL_STATE *state)
{
	if (_global_logLevel && LOG_Debug(state, __func__, DBG_STATUS))
		return state->iErrorClass;

	return state->iFirmwareVersion;
}

int DP_InitPrinter(DPAL_STATE *state, bool bufferWaitMode, int interfaceNum, int *filmRecorderID)
{
	int i, j, err;
	char str[200];
	DIR *dirp;
	struct dirent *dp;
//	FILE *fp;
//	static int isRepeatInit = 0;
	const char *FilmTypeStrs[] = { "Pack Film", "35mm", "Auto Film", "4x5", "6x7", "6x8" };

	_global_numKnownFilmTypes = 0;

	// Clear the state block
	memset(state, '\0', sizeof(*state));

	// Store the driver version information
	strcpy(state->saDriverVersion, "DPalette Driver V6.0");

#if 0
	i = GetWindowsDirectory(_global_windowsDirectory, sizeof(_global_windowsDirectory));
	if ((i <= 0) || (i >= 180)) {
		strcpy(str, "pfr.ini");
	} else {
		strcpy(str, _global_windowsDirectory);
		strcat(str, "\\pfr.ini");
	}

	if (FindFirstFile(str, &finddata) == INVALID_HANDLE_VALUE) {
		// INI file doesn't exist, create it
		fp = fopen(str, "w+t");
		if (fp == NULL) {
			// error creating INI file
			// FIXME: AAARGH!
			fclose(0);
			dp_SetErrorWritingINI(state);
			// FIXME: should return here!
		}
		fclose(fp);

		// FIXME: could probably use WritePrivateProfileInt here
		sprintf(str, "%d", interfaceNum);
		if (!WritePrivateProfileString("Initialization", "Interface", str, "pfr.ini"))
			dp_SetErrorWritingINI(state);
		if (!WritePrivateProfileString("Initialization", "Log", "0", "pfr.ini"))
			dp_SetErrorWritingINI(state);
		if (!WritePrivateProfileString("Initialization", "LogName", "pfr.log", "pfr.ini"))
			dp_SetErrorWritingINI(state);
	} else {
		// INI file exists -- load it!
		_global_logLevel = GetPrivateProfileInt("Initialization", "Log", 0, "pfr.ini");
		if (!GetPrivateProfileString("Initialization", "LogName", "", _global_logFilename, sizeof(_global_logFilename), "pfr.ini")) {
			state->iErrorClass = -22;
			state->iErrorNumber = 4;
			strcpy(state->sErrorMsg, "Can not retrieve  log file name from the PFR.INI file");
			return state->iErrorClass;
		}

		if (_global_logLevel) {
			if (isRepeatInit) {
				if (LOG_Debug(state, "Digital Palette Initialization", DBG_STATUS))
					return state->iErrorClass;
			} else {
				if (LOG_Debug(state, "Digital Palette Initialization", DBG_TIMESTAMPED))
					return state->iErrorClass;
				isRepeatInit++;
			}
		}
	}
#endif

	strcat(state->saDriverVersion, " ");
	strcat(state->saDriverVersion, "ASPI SCSI");

	if (interfaceNum)
		state->iPort = interfaceNum;
	else
		state->iPort = 17;

	err = DP_scsi_init(state, filmRecorderID);
	if (err)
		return state->iErrorClass;

	DP_GetPrinterStatus(state, 1);		// FIXME: MAGIC NUMBER
	if (state->iErrorClass >= -7) {
		if (state->iErrorClass <= -3) {
			return state->iErrorClass;
		} else {
			if ((state->iErrorNumber != 0x2600) && (state->iErrorNumber != 0x2701))
				return state->iErrorClass;
		}
	}

	state->field_471 = 1;

	int CFRModel = -1;
	// FIXME in this block: magic numbers!
	if (state->iFirmwareVersion >= 500) {
		if ((state->iFirmwareVersion <= 700) || (state->iFirmwareVersion >= 800)) {
			if (state->iFirmwareVersion <= 520)
				CFRModel = DP_MODEL_HR6000;
			else
				CFRModel = DP_MODEL_PP8000;
		} else {
			CFRModel = DP_MODEL_PP7000;
		}
	} else {
		CFRModel = DP_MODEL_CI5000;
	}

	if (GetFilmTablePath(CFRModel, _global_filmProfilePath, sizeof(_global_filmProfilePath))) {
		state->iErrorClass = -22;
		state->iErrorNumber = 4;
		strcpy(state->sErrorMsg, "Can not retrieve PATH to the film tables directory");
		return state->iErrorClass;
	}

	strcpy(str, _global_filmProfilePath);

	if ((dirp = opendir(_global_filmProfilePath)) == NULL) {
		state->iErrorClass = -22;
		state->iErrorNumber = 5;
		sprintf(state->sErrorMsg, "No film table file found");
		return state->iErrorClass;
	}

	DP_GetPrinterStatus(state, 2);
	if (state->iErrorClass)
		return state->iErrorClass;

	bool v17 = false;
	while ((dp = readdir(dirp)) != NULL) {
		char filmtableFilename[132];	// FIXME may be 128 plus padding
		size_t filmtableFileLen;
		unsigned char buf, byte10016E10;

		// Skip files which are not film tables
		char *p = strrchr(dp->d_name, '.');
		if (p == NULL)
			continue;
		if (strcasecmp(p, ".flm") != 0)
			continue;

		snprintf(filmtableFilename, sizeof(filmtableFilename), "%s/%s", _global_filmProfilePath, dp->d_name);
		strcpy(state->saFilmFile, dp->d_name);

		if (dp_open_film_table(state, filmtableFilename)) {
			state->iErrorClass = -6;
			state->iErrorNumber = 5;
			sprintf(state->sErrorMsg, "Error opening film table file %s", state->saFilmFile);
			return state->iErrorClass;
		}

		// Get the length of the filmtable file
		fseek(state->FFilmTable, 0, SEEK_END);
		filmtableFileLen = ftell(state->FFilmTable);
		fseek(state->FFilmTable, 0, SEEK_SET);

		state->saFilmName[1] = 'D';
		// byte_10016E24 = 0; // not used anywhere but here
		filmtable_crypto_init();

		for (i=0; i<24; i++) {
			if (fscanf(state->FFilmTable, "%c", &buf) == -1) {
				dp_close_filmtable(state);
				state->iErrorClass = -6;
				state->iErrorNumber = 6;
				sprintf(state->sErrorMsg, "Error reading film table file %s", state->saFilmFile);
				return state->iErrorClass;
			}
			state->saFilmName[i+3] = filmtable_crypto_decrypt(buf);
		}

		if (fscanf(state->FFilmTable, "%c", &buf) == -1) {
			dp_close_filmtable(state);
			state->iErrorClass = -6;
			state->iErrorNumber = 6;
			sprintf(state->sErrorMsg, "Error reading film table file %s", state->saFilmFile);
			return state->iErrorClass;
		}
		byte10016E10 = filmtable_crypto_decrypt(buf);
		state->saFilmName[2] = ' ';

		if (strcmp(state->saCameraType, FilmTypeStrs[byte10016E10]) == 0) {
			for (i=3; i<strlen(state->saFilmName)-1; i++) {
				if (state->saFilmName[i] == 'v') {
					if ((DP_firmware_rev(state) > 700) && (DP_firmware_rev(state) < 800) && state->saFilmName[i+1] == '7') {
						// Profile is for the ProPalette 7000 (?)
						state->saFilmName[2] = '*';
						break;
					}

					if ((((DP_firmware_rev(state) > 520) && (DP_firmware_rev(state) < 700)) || DP_firmware_rev(state) > 800) && state->saFilmName[i+1] == 'P') {
						// Profile is for the ProPalette 8000 (?)
						state->saFilmName[2] = '*';
						break;
					}

					if ((DP_firmware_rev(state) > 500) && (DP_firmware_rev(state) < 521) && state->saFilmName[i+1] == 'H') {
						// Profile is for the HR-6000 (?)
						state->saFilmName[2] = '*';
						break;
					}

					if ((DP_firmware_rev(state) < 500) && state->saFilmName[i+12] == ' ') {
						// Profile is for the CI-5000 (?)
						state->saFilmName[2] = '*';
						break;
					}
				}
			}
		}

		if (fscanf(state->FFilmTable, "%c", &buf) == -1) {
			dp_close_filmtable(state);
			state->iErrorClass = -6;
			state->iErrorNumber = 6;
			sprintf(state->sErrorMsg, "Error reading film table file %s", state->saFilmFile);
			return state->iErrorClass;
		}
		byte10016E10 = filmtable_crypto_decrypt(buf);
		if (byte10016E10 & 0x10)
			state->saFilmName[0] = 'B';
		else
			state->saFilmName[0] = ' ';

		dp_close_filmtable(state);

		if (state->saFilmName[2] == '*' && !v17) {
			// I assume this means "film table is valid for this machine" and "no filmtable loaded yet"
			v17 = true;

			if (dp_open_film_table(state, filmtableFilename)) {
				state->iErrorClass = -6;
				state->iErrorNumber = 5;
				sprintf(state->sErrorMsg, "Error opening film table file %s", state->saFilmFile);
				return state->iErrorClass;
			}

			fseek(state->FFilmTable, 0, SEEK_END);
			filmtableFileLen = ftell(state->FFilmTable);
			fseek(state->FFilmTable, 0, SEEK_SET);

			char *filmtable =malloc(filmtableFileLen + 2);
			*filmtable = _global_byte10014288;

			if (dp_read_filmtable_bytes(state, (unsigned char *)filmtable+1, filmtableFileLen) != filmtableFileLen) {
				dp_close_filmtable(state);
				free(filmtable);
				state->iErrorClass = -6;
				state->iErrorNumber = 6;
				sprintf(state->sErrorMsg, "Error reading film table file %s", state->saFilmFile);
				return state->iErrorClass;
			}

			dp_close_filmtable(state);

			if (waitAndUpdateBufferFree(state, filmtableFileLen+6)) {
				state->iErrorClass = -15;
				state->iErrorNumber = 7;
				strcpy(state->sErrorMsg, "Available buffer space smaller than film table file");
				return state->iErrorClass;
			}

#if 0
			if (_global_PortLt16) {
				free(filmtable);
				state->iErrorClass = -21;
				state->iErrorNumber = 8;
				strcpy(state->sErrorMsg, "Unknown (wrong) port specifeid for communication");
				return state->iErrorClass;
			}
#endif

			state->iErrorClass = DP_doscsi_cmd(state, 0x0C, (unsigned char *)filmtable, filmtableFileLen + 1, 10, 0, 1);
			free(filmtable);
			if (state->iErrorClass || DP_GetPrinterStatus(state, 1))
				return state->iErrorClass;

			state->iErrorClass = DP_doscsi_cmd(state, 0x0C, (unsigned char *)state->saFilmName, (_global_byte10014288 << 8) | 24, 4, 0, 0);

			if (state->iErrorClass || DP_GetPrinterStatus(state, 1))
				return state->iErrorClass;
		}

		_global_numKnownFilmTypes++;
		FILMTABLE *ft = realloc(_global_films, _global_numKnownFilmTypes * sizeof(_global_films[0]));
		if (ft == NULL) {
			// FIXME: ERROR: out of memory
		}
		_global_films = ft;
		ft += _global_numKnownFilmTypes-1;
		strcpy(ft->filmFile, state->saFilmFile);
		strcpy(ft->filmName, state->saFilmName);
	}

	closedir(dirp);

	state->iErrorClass = DP_doscsi_cmd(state, 0x0C, 0, 0, 7, 0, 3);

	if (state->iErrorClass) {
		return DP_GetPrinterStatus(state, 1);
	} else {
		strcpy(state->saMagicString, "Digital Palette");
		state->iLineLength = 2048;
		state->iHorRes = state->iLineLength;
		state->iVerRes = 1365;
		state->iHorOff = 0;
		state->iVerOff = 0;
		state->field_30[0] = 0;
		state->field_30[1] = 0;
		state->field_30[2] = 0;
		state->field_30[3] = 0;
		state->cLiteDark = 3;
		state->ucaExposTimeBlue = 100;
		state->ucaExposTimeGreen = state->ucaExposTimeBlue;
		state->ucaExposTimeRed = state->ucaExposTimeGreen;
		state->ucaLuminantBlue = 100;
		state->ucaLuminantGreen = state->ucaLuminantBlue;
		state->ucaLuminantRed = state->ucaLuminantGreen;
		state->caColorBalBlue = 3;
		state->caColorBalGreen = state->caColorBalBlue;
		state->caColorBalRed = state->caColorBalGreen;
		state->caCamAdjustZ = 0;
		state->caCamAdjustY = state->caCamAdjustZ;
		state->caCamAdjustX = state->caCamAdjustY;
		state->UNK_numTotalLines = state->iVerRes - 2 * state->iVerOff;// TODO: vertical centering?
		state->cServo = 4;
		for (j=0; j<3; j++) {
			for ( i = 0; (signed int)i < 256; i++ )
				state->ColourLUT[(256 * j) + i] = i;
		}
		state->field_75[0] = 0;
		state->field_75[1] = 0;
		for (j=0; j<3; j++) {
			state->field_75[j + 2] = 0;
			state->field_75[j + 5] = 0;
			state->field_75[j + 8] = 0;
			state->field_75[j + 11] = 0;
		}
		state->cImageCompression = 0;
		state->field_4D4[0] = 0;
		state->field_4D4[1] = 0;
		state->field_4D8[0] = 100;
		state->field_4D8[1] = 600;
		state->field_4D8[2] = 1200;
		state->bBufferWait = bufferWaitMode;
		state->field_4C0 = 800;
		DP_ExposureWarning(state);
		if ( (state->bBufferWait || state->iErrorClass <= 0) && state->iErrorClass >= 0 ) {
			DP_GetPrinterStatus(state, 0x80FFu);
			if (state->iErrorClass) {
				return state->iErrorClass;
			} else {
				if (state->iErrorClass)
					state->field_470 = 0;
				else
					state->field_470 = 1;
				return state->iErrorClass;
			}
		} else {
			return DP_GetPrinterStatus(state, 1);
		}
	}

	return state->iErrorClass;
}

int DP_InqBlockMode(DPAL_STATE *state, unsigned int *bmData)
{
	unsigned char buf[8];
	int i;

	state->iErrorClass = DP_doscsi_cmd(state, 0x0C, buf, sizeof(buf), 21, 0, 0);
	if (state->iErrorClass) {
		state->iErrorNumber = -2;
		strcpy(state->sErrorMsg, "Block mode. Inquiry command error");
		//return DP_GetPrinterStatus(state, 1);	// FIXME MAGIC NUMBER
		return state->iErrorClass;
	}

	for (i=0; i<8; i+=2) {
		bmData[i/2] = ((buf[i] << 8) | buf[i+1]);
	}

	return 0;
}

int DP_Pacing(DPAL_STATE *state, int pacing)
{
	char buf[2];

	buf[0] = (pacing >> 8) & 0xff;
	buf[1] = pacing & 0xff;
	DP_doscsi_cmd(state, 0x0C, buf, sizeof(buf), 31, 0, 1);
	return state->iErrorClass;
}

int DP_ResetToDefault(DPAL_STATE *state)
{
	if (_global_logLevel && LOG_Debug(state, __func__, DBG_STATUS))
		return state->iErrorClass;

	state->iErrorClass = DP_doscsi_cmd(state, 0x0C, 0, 0, 7, 0, 3);		// FIXME magic numbers

	if (state->iErrorClass)
		return DP_GetPrinterStatus(state, 1);
	else
		return 0;
}

int DP_ExposureWarning(DPAL_STATE *state)
{
	if (_global_logLevel && LOG_Debug(state, __func__, DBG_STATUS))
		return state->iErrorClass;

	if (DP_GetPrinterStatus(state, 8))		// FIXME magic number
		return state->iErrorClass;

	if (state->ucStatusByte0 & 0x04)		// FIXME magic number
		return state->iErrorClass;

	while (state->ucStatusByte0 & 0x02) {	// FIXME magic number
		if (DP_GetPrinterStatus(state, 8))	// FIXME magic number
			return state->iErrorClass;
		delaySeconds(1);
	}

	return state->iErrorClass;
}

int DP_ShutDown(DPAL_STATE *state)
{
	if (_global_logLevel)
		LOG_Debug(state, __func__, DBG_STATUS);

	return DP_TerminateExposure(state, 1);
}

int FilmTableName(DPAL_STATE *state, int filmRecorderId, int filmType, char *buf)
{
	if (_global_logLevel && LOG_Debug(state, __func__, DBG_STATUS))
		return state->iErrorClass;

	if ((filmType < 0) || (filmType >= _global_numKnownFilmTypes)) {
		state->iErrorClass = -8;
		state->iErrorNumber = 3;
		strcpy(state->sErrorMsg, "Wrong film table number in the FilmTableName() function call");
		*buf = '\0';
	}

	strcpy(buf, _global_films[filmType].filmName);
	return 0;
}

int NumberFilmTables(int *pNumFilmTables)
{
	if (_global_numKnownFilmTypes) {
		*pNumFilmTables = _global_numKnownFilmTypes;
		return 0;
	} else {
		*pNumFilmTables = -1;
		return -1;
	}
}

int DP_TerminateExposure(DPAL_STATE *state, int a2)
{
	int var_4 = 0;

	if (_global_logLevel && LOG_Debug(state, __func__, DBG_STATUS))
			return state->iErrorClass;

	if ((a2 < 0) || (a2 > 2)) {
		state->iErrorClass = -7;
		state->iErrorNumber = 2;
		if (_global_logLevel)
			LOG_Error(state, __func__, ERR_STATUS);
		return DP_GetPrinterStatus(state, 1);
	}

	if (!a2) {	// 10001CF6 compare
		// 10001d00 true
		state->iErrorClass = DP_doscsi_cmd(state, 0x0C, 0, 0, 3, 0, 3);
	} else {
		// 10001d47 false --> a2 is zero
		if (DP_firmware_rev(state) == 170) {
			// 10001d5b DP_firmware_rev eq zero
			if (DP_GetPrinterStatus(state, 8)) {
				if (_global_logLevel)
					LOG_Error(state, __func__, ERR_STATUS);
				// 10001d93 exit case
				return state->iErrorClass;
			}
			// 10001da2 getprinterstatus returned zero
			if (state->ucStatusByte0 & 2) {
				var_4 = 3;
			}

			while (state->iBufferFree == 0) {
				DP_GetPrinterStatus(state, 64);
				delaySeconds(1);
			}
		}

		// 10001df6
		state->iErrorClass = DP_doscsi_cmd(state, 0x1B, 0, 0, 0, 0, 3);		// SCSI START/STOP UNIT

		// 10001e29
		if ((var_4 == 3) || ((state->iErrorClass != 0) && (DP_firmware_rev(state) == 170))) {
			// 10001e57 film is dirty
			state->iErrorClass = -2;
			state->iErrorNumber = 9227;
			strcpy(state->sErrorMsg, "Film is Dirty");
		}
	}

	// 100018e7 post compare
	if (state->iErrorClass) {
		if (_global_logLevel)
			LOG_Error(state, __func__, ERR_STATUS);
		return DP_GetPrinterStatus(state, 1);
	}

	// 10001EC9 errorclass = 0
	DP_ExposureWarning(state);
	if (state->iErrorClass == 0) {
		if (state->ucStatusByte0 & 4)
			state->iErrorClass = 3;
	}

	return DP_GetPrinterStatus(state, 1);
}

int ToolKitLog(DPAL_STATE *state, DP_LOG_LEVEL verbosity)
{
	switch (verbosity) {
	case DP_LOG_NONE:
	case DP_LOG_SIMPLE:
	case DP_LOG_ADVANCED:
		_global_logLevel = verbosity;
		state->saMagicString[0] = verbosity;		// FIXME may not be required, Polaroid code does this
		break;
	case DP_LOG_GET:
		break;
	}

	return _global_logLevel;
}

int DP_DownLoadFilms(DPAL_STATE *state, int filmRecorderId, int filmType, unsigned char a4)
{
	char fileName[200];
	size_t fileLen;

	if (_global_logLevel)
		if (LOG_Debug(state, __func__, DBG_STATUS))
			return state->iErrorClass;

	if ((filmType > _global_numKnownFilmTypes) || (filmType < 0)) {
		state->iErrorClass = -8;
		state->iErrorNumber = 3;
		strcpy(state->sErrorMsg, "Wrong film table number in the FilmTableName() function call");	// FIXME blatantly wrong!
		return state->iErrorClass;
	}

	strcpy(state->saFilmFile, _global_films[filmType].filmFile);
	strcpy(state->saFilmName, _global_films[filmType].filmName);
	strcpy(fileName, _global_filmProfilePath);
	strcat(fileName, "\\");
	strcat(fileName, state->saFilmFile);

	if (dp_open_film_table(state, fileName)) {
		state->iErrorClass = -6;
		state->iErrorNumber = 5;
		sprintf(state->sErrorMsg, "Error opening film table file %s", state->saFilmFile);
		return state->iErrorClass;
	}

	fseek(state->FFilmTable, 0, SEEK_END);
	fileLen = ftell(state->FFilmTable);
	fseek(state->FFilmTable, 0, SEEK_SET);

	unsigned char *buf = malloc(fileLen + 2);
	buf[0] = a4;
	_global_byte10014288 = a4;

	if (dp_read_filmtable_bytes(state, &buf[1], fileLen) != fileLen) {
		dp_close_filmtable(state);
		free(buf);
		state->iErrorClass = -6;
		state->iErrorNumber = 6;
		sprintf(state->sErrorMsg, "Error reading film table file %s", state->saFilmFile);
		return state->iErrorClass;
	}

	dp_close_filmtable(state);

	if (waitAndUpdateBufferFree(state, fileLen + 6)) {
		state->iErrorClass = -15;
		state->iErrorNumber = 7;
		strcpy(state->sErrorMsg, "Available buffer space smaller than film table file");
		return state->iErrorClass;
	}

#if 0
	if (_global_PortLt16) {
		free(buf);
		state->iErrorClass = -21;
		state->iErrorNumber = 8;
		strcpy(state->sErrorMsg, "Unknown (wrong) port specifeid for communication");
		return state->iErrorClass;
	}
#endif

	state->iErrorClass = DP_doscsi_cmd(state, 0x0C, buf, fileLen + 1, 10, 0, 1);
	free(buf);
	if (state->iErrorClass || DP_GetPrinterStatus(state, 1))
		return state->iErrorClass;

	DP_doscsi_cmd(state, 0x0C, state->caAspectRatio, (_global_byte10014288 << 8) + sizeof(state->caAspectRatio), 5, 0, 0);

	state->caCamAdjustX = 0;
	state->caCamAdjustY = 0;
	state->caCamAdjustZ = 0;

	state->ucaExposTimeRed = 100;
	state->ucaExposTimeGreen = 100;
	state->ucaExposTimeBlue = 100;

	state->ucaLuminantRed = 100;
	state->ucaLuminantGreen = 100;
	state->ucaLuminantBlue = 100;

	return 0;
}
