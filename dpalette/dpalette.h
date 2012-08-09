#ifndef DPALETTE_H
#define DPALETTE_H

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
Polaroid seem to make extensive use of Hungarian notation, or a variant thereof.
Prefixes known or guessed:
  sa		String Array
  i			Integer				(32 bit signed)
  ui		Unsigned Integer	(32 bit unsigned)
  c			Char				(8 bit signed)
  ca		Char Array
  uc		Unsigned Char		(8 bit unsigned)
  uca		Unsigned Char Array

  field_* fields have memory allocated but have an unknown purpose.
 */
typedef struct {
	char	saMagicString[24];
	int		iPort;
	int		iHorRes, iVerRes;
	int		iLineLength;
	int		iHorOff, iVerOff;
	char	field_30[4];
	char	cLiteDark;
	unsigned char	ucaExposTimeRed, ucaExposTimeGreen, ucaExposTimeBlue;
	unsigned char	ucaLuminantRed, ucaLuminantGreen, ucaLuminantBlue;
	char	caColorBalRed, caColorBalGreen, caColorBalBlue;
	char	caCamAdjustX, caCamAdjustY, caCamAdjustZ;
	char	saFilmFile[13];
	char	saFilmName[27];				// FIXME may only be 24 chars in length
	unsigned char	ucaFilmNumber;
	char	field_6A[6];								// never used
	int		UNK_numTotalLines;							// field_70
	char	cServo;
	char	field_75[14];								//
	char	ColourLUT[768];								// three 256-entry 8bit->8bit colour channel LUTs - in order: red, green, blue
	char	cImageCompression;
	char	ucStatusByte0, ucStatusByte1;
	char	field_386[2];								// never used
	int		uiExposedLines;
	char	saExposureStatus[64];						// seems to be a status string; updated by DP_GetPrinterStatus if mode&0x48 != 0
	char	field_3CC;									// set by decode_scsi_inquiry_scb
	//char	saFirmwareVersion[64];
	int		iFirmwareVersion;
	char	caAspectRatio[2];
	char	field_40F;									// assigned to zero by DP_GetPrinterStatus, otherwise unused
	int		iBufferFree;
	int		field_414;									// set and used by decode_scsi_inquiry_scb
	char	field_418;									// never used, probably padding
	char	ucCameraCode;
	char	saCameraType[14];							// this is either 14 bytes, or 12 bytes with some padding
	int		iErrorNumber;
	char	sErrorMsg[64];
	int		iErrorClass;
	char	field_470;									// seems to be an "initialisation successful" flag?
	char	field_471;									// seems to be a "film recorder detected" flag?
	char	field_472[22];								// apparently never used
	bool	bBufferWait;								// TRUE  -- DP_SendImageData/SemdImageBlock/DownLoadFilms will wait if the buffer is full
														// FALSE -- they will return immediately with status == 1
	char	field_489[11];								// apparently never used
	char	saDriverVersion[40];
	FILE 	*FFilmTable;
	int		field_4C0;
	int		field_4C4;
	int		field_4C8[2];
	int		field_4D0;
	char	field_4D4[4];
	int		field_4D8[3];
} DPAL_STATE;

/// Digital Palette models
typedef enum {
	DP_MODEL_CI5000 = 0,		///< CI-5000
	DP_MODEL_HR6000 = 1,		///< HR-6000
	DP_MODEL_PP7000 = 2,		///< ProPalette 7000
	DP_MODEL_PP8000 = 3			///< ProPalette 8000
} DP_MODEL;

typedef enum {
	DP_LOG_NONE = 0,			///< No logging
	DP_LOG_SIMPLE = 1,			///< Simple logging
	DP_LOG_ADVANCED = 2,		///< Advanced logging
	DP_LOG_GET = 3				///< Retrieve current log level from INI file
} DP_LOG_LEVEL;

int DP_DownLoadFilms(DPAL_STATE *state, int filmRecorderId, int filmType, unsigned char a4);
int DP_ExposureWarning(DPAL_STATE *state);
// DP_FirmWareBurn --> not implemented (intentionally!)
// DP_FirmWareLoad --> not implemented (intentionally!)
// DP_FirmWareStart --> not implemented (intentionally!)
int DP_GetPrinterStatus(DPAL_STATE *state, int mode);
int DP_InitPrinter(DPAL_STATE *state, bool bufferWaitMode, int interfaceNum, int *filmRecorderID);
/// Returns 4x 16-bit integers in bmData
int DP_InqBlockMode(DPAL_STATE *state, unsigned int *bmData);
int DP_Pacing(DPAL_STATE *state, int pacing);
int DP_ResetToDefault(DPAL_STATE *state);
// DP_SendImageBlock
int DP_SendImageData(DPAL_STATE *state, const unsigned int lineNumber, const unsigned char *src, const size_t size, unsigned char flags);
int DP_SendPrinterParams(DPAL_STATE *state);
int DP_ShutDown(DPAL_STATE *state);
int DP_StartExposure(DPAL_STATE *state);
int DP_TerminateExposure(DPAL_STATE *state, int a2);
// DP_doscsi_cmd --> not going to export this :)
int DP_firmware_rev(DPAL_STATE *state);
// DP_scsi_init --> not going to export this :)
// DP_scsi_inq --> not going to export this :)
int FilmTableName(DPAL_STATE *state, int filmRecorderId, int filmType, char *buf);
int NumberFilmTables(int *pNumFilmTables);
int ToolKitLog(DPAL_STATE *state, DP_LOG_LEVEL verbosity);
// firmware_rev --> not going to export this :)

#ifdef __cplusplus
}
#endif

#endif // DPALETTE_H
