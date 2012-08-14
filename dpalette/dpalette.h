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
	char	sDeviceName[24];
	int		iDeviceFd;					// File descriptor associated with the SCSI device
	int		iHorRes, iVerRes;
	int		iLineLength;
	int		iHorOff, iVerOff;
//	char	field_30[4];
	char	clp, clu, cBu, cCal;
	char	cLiteDark;
	unsigned char	ucaExposTimeRed, ucaExposTimeGreen, ucaExposTimeBlue;
	unsigned char	ucaLuminantRed, ucaLuminantGreen, ucaLuminantBlue;
	char	caColorBalRed, caColorBalGreen, caColorBalBlue;
	char	caCamAdjustX, caCamAdjustY, caCamAdjustZ;
	char	saFilmFile[13];
	char	saFilmName[27];				// 23 character film name plus three status characters and a NULL
	unsigned char	ucaFilmNumber;
	int		iVertHeight;							// field_70
	char	cServo;
//	char	field_75[14];								//
	char	cBkgndMode;
	unsigned char	ucBkgndValue;
	unsigned char	ucaUl_corner_color[3];
	unsigned char	ucaUr_corner_color[3];
	unsigned char	ucaLl_corner_color[3];
	unsigned char	ucaLr_corner_color[3];
	char	ColourLUT[768];								// three 256-entry 8bit->8bit colour channel LUTs - in order: red, green, blue
	char	cImageCompression;
	unsigned char	ucStatusByte0, ucStatusByte1;
	int		uiExposedLines;
	char	sStatusMsg[64];						// seems to be a status string; updated by DP_GetPrinterStatus if mode&0x48 != 0
	unsigned char	ucOptionByte0;									// set by decode_scsi_inquiry_scb
	//char	saFirmwareVersion[64];
	int		iFirmwareVersion;
	char	caAspectRatio[2];
	int		iBufferFree;
	int		iBufferTotal;									// set and used by decode_scsi_inquiry_scb
	char	cBufferMsgTerm;									// never used, probably padding
	unsigned char	ucCameraCode;
	char	sCameraMsg[14];							// this is either 14 bytes, or 12 bytes with some padding
	int		iErrorNumber;
	char	sErrorMsg[64];
	int		iErrorClass;
	char	cDPinitialized;									// seems to be an "initialisation successful" flag?
	char	cDPfound;									// seems to be a "film recorder detected" flag?
	bool	bBufferWait;								// TRUE  -- DP_SendImageData/SemdImageBlock/DownLoadFilms will wait if the buffer is full
														// FALSE -- they will return immediately with status == 1
	char	sTKversion[40];
	FILE 	*FFilmTable;
	int		line_double_threshold;
	int		iMaxHorRes;
	unsigned int	uitimes_out_of_data, uiAutoluma_whoops;
	int		iS_series_installed;
	char	cExp_Fix_Sticky, cExp_Fix_Type;
	unsigned int	uiExp_Fix_Min_Scans, uiExp_Fix_Max_Luminant, uiExp_Fix_Min_Hres_To_Diffuse;
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

int DP_DownLoadFilms(DPAL_STATE *state, int filmType, unsigned char filmNumber);
int DP_ExposureWarning(DPAL_STATE *state);
// DP_FirmWareBurn --> not implemented (intentionally!)
// DP_FirmWareLoad --> not implemented (intentionally!)
// DP_FirmWareStart --> not implemented (intentionally!)
int DP_GetPrinterStatus(DPAL_STATE *state, int mode);
int DP_InitPrinter(DPAL_STATE *state, bool bufferWaitMode, char *filmRecorderID);
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
int FilmTableName(DPAL_STATE *state, int filmType, char *buf);
int NumberFilmTables(int *pNumFilmTables);
int ToolKitLog(DPAL_STATE *state, DP_LOG_LEVEL verbosity);
// firmware_rev --> not going to export this :)

#ifdef __cplusplus
}
#endif

#endif // DPALETTE_H
