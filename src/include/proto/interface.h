#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "common/messages/messages.h"
#include "common/comm_connectivity.h"
#include "plc/message_fns.h"
#include "plc/containers.h"

#include "postgres.h"
#include "utils/builtins.h"
#include "catalog/pg_proc.h"
#include "utils/guc.h"
#include "access/transam.h"
#include "utils/array.h"
#include "utils/syscache.h"
#include "utils/array.h"

typedef struct PLCoordinatorServer {
    const char *address;
    void *server;
} PLCoordinatorServer;

// C interface definition
Datum plcontainer_function_handler(FunctionCallInfo fcinfo, plcProcInfo *proc, MemoryContext function_cxt);
 
PLCoordinatorServer *start_server(const char *address);
int process_request(PLCoordinatorServer *server, int timeout_seconds);

int get_new_container_from_coordinator(const char *runtime_id, plcContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
