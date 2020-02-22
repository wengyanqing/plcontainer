#ifndef __PLC_EXECUTOR_H__
#define __PLC_EXECUTOR_H__

#include "postgres.h"
#include "executor/execdesc.h"
#include "nodes/parsenodes.h"

#include "nodePlcscan.h"

TupleTableSlot *
PlcExecScan(PlcScanState* node, ExecScanAccessMtd accessMtd, ExecScanRecheckMtd recheckMtd);

#endif
