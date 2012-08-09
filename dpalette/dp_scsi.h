#ifndef DP_SCSI_H
#define DP_SCSI_H

#include <sys/types.h>
#include "dpalette.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DP_SCSI_INIT_GetRecorderCount 0x8000
#define DP_PORT_SCSI 16

typedef enum {
	SCSI_DIR_INPUT  = 0,
	SCSI_DIR_OUTPUT = 1,
	SCSI_DIR_NONE   = 3
} DP_SCSI_DIRECTION;

int DP_scsi_init(DPAL_STATE *state, int *filmRecorderID);
int DP_doscsi_cmd(DPAL_STATE *state, int scsiCmd, void *SRB_Buffer, size_t szSRB_Buffer, int scsiPageCode, char vendorControlBits, DP_SCSI_DIRECTION dir);
int DP_scsi_inq(DPAL_STATE *state);

#ifdef __cplusplus
}
#endif

#endif // DP_SCSI_H
