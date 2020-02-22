#include "postgres.h"

#include "access/relscan.h"
#include "executor/execdebug.h"
#include "executor/nodeSeqscan.h"
#include "utils/rel.h"

#include "nodes/extensible.h"
#include "executor/nodeCustom.h"
#include "utils/memutils.h"

#include "nodePlcscan.h"

/* CustomScanMethods */
static Node *CreatePlcScanState(CustomScan *custom_plan);

/* CustomScanExecMethods */
static void BeginPlcScan(CustomScanState *node, EState *estate, int eflags);
static void ReScanPlcScan(CustomScanState *node);
static TupleTableSlot *ExecPlcScan(CustomScanState *node);
static void EndPlcScan(CustomScanState *node);

static CustomScanMethods    plc_scan_methods = {
    "plcscan",              /* CustomName */
    CreatePlcScanState,     /* CreateCustomScanState */
};


static CustomExecMethods    plcscan_exec_methods = {
    "plcscan",              /* CustomName */
    BeginPlcScan,           /* BeginCustomScan */
    ExecPlcScan,            /* ExecCustomScan */
    EndPlcScan,             /* EndCustomScan */
    ReScanPlcScan,          /* ReScanCustomScan */
    NULL,                   /* MarkPosCustomScan */
    NULL,                   /* RestrPosCustomScan */
    NULL,                   /* EstimateDSMCustomScan */
    NULL,                   /* InitializeDSMCustomScan */
    NULL,                   /* InitializeWorkerCustomScan */
    NULL,                   /* ExplainCustomScan */
};

/*
 *  BeginPlcScan - A method of CustomScanState; that initializes
 *  the supplied PlcScanState object, at beginning of the executor.
 */ 
static void
BeginPlcScan(CustomScanState *css, EState *estate, int eflags)
{
    elog(LOG, "BeginPlcScan call");

}

/*
 *  ReScanVectorScan - A method of CustomScanState; that rewind the current
 *  seek position.
 *  
 *  Derived from ExecReScanSeqScan().
 */ 
static void
ReScanPlcScan(CustomScanState *node)
{
    elog(LOG, "ReScanPlcScan call");

}

/*
 *  ExecVectorScan - A method of CustomScanState; that fetches a tuple
 *  from the relation, if exist anymore.
 *  
 *  Derived from ExecSeqScan().
 */ 
static TupleTableSlot *
ExecPlcScan(CustomScanState *node)
{
    elog(LOG, "ExecPlcScan call");
    return NULL;
}

/*
 *  EndCustomScan - A method of CustomScanState; that closes heap and
 *  scan descriptor, and release other related resources.
 *  
 *  Derived from ExecEndSeqScan().
 */ 
static void
EndPlcScan(CustomScanState *node)
{
    elog(LOG, "EndPlcScan call");
}

/*
 *  CreatePlcScanState - A method of CustomScan; that populate a custom
 *  object being delivered from CustomScanState type, according to the
 *  supplied CustomPath object.
 */ 
static Node *
CreatePlcScanState(CustomScan *custom_plan)
{
    PlcScanState   *vss = MemoryContextAllocZero(CurTransactionContext, sizeof(PlcScanState));
    /* Set tag and executor callbacks */
    NodeSetTag(vss, T_CustomScanState);

    vss->css.methods = &plcscan_exec_methods;

    return (Node *) vss;
}

/*
 * Interface to get the custom scan plan for plcontainer scan
 */
CustomScan *
MakeCustomScanForSeqScan(void)
{
    CustomScan *cscan = (CustomScan *)makeNode(CustomScan);
    cscan->methods = &plc_scan_methods;

    return cscan;
}

/*
 *  * Initialize plcontainer CustomScan node.
 *   */
void
InitPlcScan(void)
{
    /* Register a plcscan type of custom scan node */
    RegisterCustomScanMethods(&plc_scan_methods);
}
