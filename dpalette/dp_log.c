#include <stdio.h>
#include <time.h>
#include "dp_log.h"

int LOG_Debug(DPAL_STATE *state, const char *message, const DEBUG_TYPE mode)
{
#ifndef LOG_TO_STDOUT
	char filename[180];
#endif
	time_t t;
	struct tm *tmp;
	char buf[128];
	FILE *logfile;

#ifndef LOG_TO_STDOUT
	// Generate log filename
	strcpy(filename, _global_windowsDirectory);
	strcat(filename, "\\");
	strcat(filename, _global_logFilename);

	// Open log file for appending
	logfile = fopen(filename, "a");
	if (logfile == NULL) {
		state->iErrorClass = -22;
		state->iErrorNumber = 10;
		strcpy(state->sErrorMsg, "Can not open log file");
		return state->iErrorClass;
	}
#else
	logfile = stdout;
#endif

	switch (mode) {
		case 0:
			// e.g.: "========== 21/02/94 === 12:14:26 ==== message ======="
			t = time(NULL);
			tmp = localtime(&t);
			strftime(buf, sizeof(buf), "========== %d-%b-%y === %H:%M:%S ==== ", tmp);
			fprintf(logfile, "%s%s =======\n", buf, message);
#ifndef LOG_TO_STDOUT
			fclose(logfile);
#endif
			break;

		case 1:
			// e.g. "#message", followed by DPalette state
			fprintf(logfile, "#%s\n    dp{\n", message);
			fprintf(logfile,
					"      iPort=%xh\n      iHorRes=%d\n      iVerRes=%d\n      iLineLength=%d\n",
					state->iPort,
					state->iHorRes,
					state->iVerRes,
					state->iLineLength);
			fprintf(logfile,
					"      iHorOff=%d\n      iVerOff=%d\n      cLiteDark=%d\n",
					state->iHorOff,
					state->iVerOff,
					state->cLiteDark);
			fprintf(logfile,
					"      ucaExposTime[RED]=%d\n      ucaExposTime[GREEN]=%d\n      ucaExposTime[BLUE]=%d\n",
					state->ucaExposTimeRed,
					state->ucaExposTimeGreen,
					state->ucaExposTimeBlue);
			fprintf(logfile,
					"      ucaLuminant[RED]=%d\n      ucaLuminant[GREEN]=%d\n      ucaLuminant[BLUE]=%d\n",
					state->ucaLuminantRed,
					state->ucaLuminantGreen,
					state->ucaLuminantBlue);
			fprintf(logfile,
					"      caColorBal[RED]=%d\n      caColorBal[GREEN]=%d\n      caColorBal[BLUE]=%d\n",
					state->caColorBalRed,
					state->caColorBalGreen,
					state->caColorBalBlue);
			fprintf(logfile,
					"      caCamAdjust[X]=%d\n      caCamAdjust[Y]=%d\n      caCamAdjust[Z]=%d\n",
					state->caCamAdjustX,
					state->caCamAdjustY,
					state->caCamAdjustZ);
			fprintf(logfile,
					"      saFilmFile=%s\n      saFilmName=%s\n      ucaFilmNumber=%d\n      iVertHeight=%d\n",
					state->saFilmFile,
					state->saFilmName,
					state->ucaFilmNumber,
					state->iVerRes);
			fprintf(logfile,
					"      cServo=%d\n      cImageCompression=%d\n      ucStatusByte0=%x\n      ucStatusByte1=%x\n",
					state->cServo,
					state->cImageCompression,
					state->ucStatusByte0,
					state->ucStatusByte1);
			fprintf(logfile,
					"      uiExposedLines=%d\n      caAspectRatio[0]=%d\n      caAspectRatio[1]=%d\n      iBufferFree=%d\n",
					state->uiExposedLines,
					state->caAspectRatio[0],
					state->caAspectRatio[1],
					state->iBufferFree);
			fprintf(logfile,
					"      ucCameraCode=%d\n      iErrorNumber=%d\n      sErrorMsg=%s\n      iErrorClass=%d\n      }\n",
					state->ucCameraCode,
					state->iErrorNumber,
					state->sErrorMsg,
					state->iErrorClass);
#ifndef LOG_TO_STDOUT
			fclose(logfile);
#endif
			break;

		case 2:
			break;

		case 3:
			fprintf(logfile, "#%s\n", message);
#ifndef LOG_TO_STDOUT
			fclose(logfile);
#endif
			break;
	}
	return 0;
}

int LOG_Error(DPAL_STATE *state, const char *message, const ERROR_TYPE mode)
{
#ifndef LOG_TO_STDOUT
	char filename[180];
#endif
	FILE *logfile;

#ifndef LOG_TO_STDOUT
	// Generate log filename
	strcpy(filename, _global_windowsDirectory);
	strcat(filename, "\\");
	strcat(filename, _global_logFilename);

	// Open log file for appending
	logfile = fopen(filename, "a");
	if (logfile == NULL) {
		state->iErrorClass = -22;
		state->iErrorNumber = 10;
		strcpy(state->sErrorMsg, "Can not open log file");
		return state->iErrorClass;
	}
#else
	logfile = stdout;
#endif

	if (mode == 0) {
		fprintf(logfile, "#%s =============== Error ==============\n    dp{\n", message);
		fprintf(logfile,
				"      iPort=%xh\n      iHorRes=%d\n      iVerRes=%d\n      iLineLength=%d\n",
				state->iPort,
				state->iHorRes,
				state->iVerRes,
				state->iLineLength);
		fprintf(logfile,
				"      iHorOff=%d\n      iVerOff=%d\n      cLiteDark=%d\n",
				state->iHorOff,
				state->iVerOff,
				state->cLiteDark);
		fprintf(logfile,
				"      ucaExposTime[RED]=%d\n      ucaExposTime[GREEN]=%d\n      ucaExposTime[BLUE]=%d\n",
				state->ucaExposTimeRed,
				state->ucaExposTimeGreen,
				state->ucaExposTimeBlue);
		fprintf(logfile,
				"      ucaLuminant[RED]=%d\n      ucaLuminant[GREEN]=%d\n      ucaLuminant[BLUE]=%d\n",
				state->ucaLuminantRed,
				state->ucaLuminantGreen,
				state->ucaLuminantBlue);
		fprintf(logfile,
				"      caColorBal[RED]=%d\n      caColorBal[GREEN]=%d\n      caColorBal[BLUE]=%d\n",
				state->caColorBalRed,
				state->caColorBalGreen,
				state->caColorBalBlue);
		fprintf(logfile,
				"      caCamAdjust[X]=%d\n      caCamAdjust[Y]=%d\n      caCamAdjust[Z]=%d\n",
				state->caCamAdjustX,
				state->caCamAdjustY,
				state->caCamAdjustZ);
		fprintf(logfile,
				"      saFilmFile=%s\n      saFilmName=%s\n      ucaFilmNumber=%d\n      iVertHeight=%d\n",
				state->saFilmFile,
				state->saFilmName,
				state->ucaFilmNumber,
				state->iVerRes);
		fprintf(logfile,
				"      cServo=%d\n      cImageCompression=%d\n      ucStatusByte0=%x\n      ucStatusByte1=%x\n",
				state->cServo,
				state->cImageCompression,
				state->ucStatusByte0,
				state->ucStatusByte1);
		fprintf(logfile,
				"      uiExposedLines=%d\n      caAspectRatio[0]=%d\n      caAspectRatio[1]=%d\n      iBufferFree=%d\n",
				state->uiExposedLines,
				state->caAspectRatio[0],
				state->caAspectRatio[1],
				state->iBufferFree);
		fprintf(logfile,
				"      ucCameraCode=%d\n      iErrorNumber=%d\n      sErrorMsg=%s\n      iErrorClass=%d\n      }\n",
				state->ucCameraCode,
				state->iErrorNumber,
				state->sErrorMsg,
				state->iErrorClass);
#ifndef LOG_TO_STDOUT
		fclose(logfile);
#endif
	} else if (mode == 1) {
		fprintf(logfile, "#%s =============== Error ==============\n", message);
#ifndef LOG_TO_STDOUT
		fclose(logfile);
#endif
	}
	return 0;
}
