#ifndef DP_LOG_H
#define DP_LOG_H

#define LOG_TO_STDOUT

#include "dpalette.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	DBG_TIMESTAMPED	= 0,
	DBG_STATUS		= 1,
	DBG_NONE		= 2,
	DBG_MESSAGE		= 3
} DEBUG_TYPE;

typedef enum {
	ERR_STATUS		= 0,
	ERR_MESSAGE		= 1
} ERROR_TYPE;

int LOG_Debug(DPAL_STATE *state, const char *message, const DEBUG_TYPE mode);
int LOG_Error(DPAL_STATE *state, const char *message, const ERROR_TYPE mode);

#ifdef __cplusplus
}
#endif

#endif // DP_LOG_H
