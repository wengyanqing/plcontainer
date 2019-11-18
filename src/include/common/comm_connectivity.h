/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_COMM_CONNECTIVITY_H
#define PLC_COMM_CONNECTIVITY_H

#include <stddef.h>
#include <stdint.h>

#define DEFAULT_STRING_BUFFER_SIZE 1024

typedef struct plcContext
{
	char *service_address; /* File for unix domain socket connection only. */
} plcContext;


#define UDS_SHARED_FILE "unix.domain.socket.shared.file"
#define IPC_CLIENT_DIR "/tmp/plcontainer"
#define IPC_GPDB_BASE_DIR "/tmp/plcontainer"
#define MAX_SHARED_FILE_SZ strlen(UDS_SHARED_FILE)

extern void plcContextInit(plcContext *ctx);
extern void plcFreeContext(plcContext *ctx);
extern void plcReleaseContext(plcContext *ctx);
extern void plcContextReset(plcContext *ctx);

#endif /* PLC_COMM_CONNECTIVITY_H */
