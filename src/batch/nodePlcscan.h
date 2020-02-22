#ifndef __PLC_SCAN_H__
#define __PLC_SCAN_H__

#include "nodes/execnodes.h"
#include "nodes/plannodes.h"

#define     PLC_BATCH_SIZE  1024

typedef enum PlcScanBatchStatus
{
    PLC_BATCH_UNSTART,
    PLC_BATCH_FETCH_FINISH,
    PLC_BATCH_SCAN_IN_PROCESS,
    PLC_BATCH_SCAN_FINISH,
    PLC_SCAN_FINISH
} PlcScanBatchStatus;

typedef struct PlcScanState
{
    CustomScanState css;

    /* Attributes for plcontainer */
    SeqScanState    *seqstate;

    TupleTableSlot  *batch[PLC_BATCH_SIZE];
    int             batch_num;
    int             batch_size;
    int             cur_batch_scan_num;
    PlcScanBatchStatus  batch_status; 
        
    bool        scanFinish;

    //MemoryContext   batchctx;
} PlcScanState;

extern CustomScan *MakeCustomScanForSeqScan(void);
extern void InitPlcScan(void);

extern PlcScanState *g_PlcScanState;
extern bool                 enable_plc_batch_mode;

#endif
