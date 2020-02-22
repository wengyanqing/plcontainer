#ifndef __PLC_SCAN_H__
#define __PLC_SCAN_H__

#include "nodes/execnodes.h"
#include "nodes/plannodes.h"

typedef struct PlcScanState
{
    CustomScanState css;

    /* Attributes for plcontainer */
    SeqScanState    *seqstate;
    bool        scanFinish;
} PlcScanState;

extern CustomScan *MakeCustomScanForSeqScan(void);
extern void InitPlcScan(void);

#endif
