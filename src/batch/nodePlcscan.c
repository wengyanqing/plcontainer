#include "postgres.h"

#include "access/relscan.h"
#include "executor/execdebug.h"
#include "executor/nodeSeqscan.h"
#include "utils/rel.h"

#include "nodes/extensible.h"
#include "executor/nodeCustom.h"
#include "utils/memutils.h"

#include "cdb/cdbappendonlyam.h"
#include "cdb/cdbaocsam.h"
#include "utils/snapmgr.h"

#include "nodePlcscan.h"
#include "executor.h"

PlcScanState *g_PlcScanState = NULL;

/* CustomScanMethods */
static Node *CreatePlcScanState(CustomScan *custom_plan);

/* CustomScanExecMethods */
static void BeginPlcScan(CustomScanState *node, EState *estate, int eflags);
static void ReScanPlcScan(CustomScanState *node);
static TupleTableSlot *ExecPlcScan(CustomScanState *node);
static void EndPlcScan(CustomScanState *node);

static TupleTableSlot *SeqNext(SeqScanState *node);
static bool SeqRecheck(SeqScanState *node, TupleTableSlot *slot);

static TupleTableSlot *
SeqNext(SeqScanState *node)
{
    HeapTuple   tuple;
    EState     *estate;
    ScanDirection direction;
    TupleTableSlot *slot;

    /*
     * get information from the estate and scan state
     */
    estate = node->ss.ps.state;
    direction = estate->es_direction;
    slot = node->ss.ss_ScanTupleSlot;

    if (node->ss_currentScanDesc_ao == NULL &&
        node->ss_currentScanDesc_aocs == NULL &&
        node->ss_currentScanDesc_heap == NULL)
    {
        Relation currentRelation = node->ss.ss_currentRelation;

        if (RelationIsAoRows(currentRelation))
        {
/*
            Snapshot appendOnlyMetaDataSnapshot;

            appendOnlyMetaDataSnapshot = node->ss.ps.state->es_snapshot;
            if (appendOnlyMetaDataSnapshot == SnapshotAny)
            {
                appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
            }

            node->ss_currentScanDesc_ao = appendonly_beginscan(
                currentRelation,
                node->ss.ps.state->es_snapshot,
                appendOnlyMetaDataSnapshot,
                0, NULL);
*/
            elog(ERROR, "ao table with plcontaienr scan not implemented.");
        }
        else if (RelationIsAoCols(currentRelation))
        {
/*
            Snapshot appendOnlyMetaDataSnapshot;

            InitAOCSScanOpaque(node, currentRelation);

            appendOnlyMetaDataSnapshot = node->ss.ps.state->es_snapshot;
            if (appendOnlyMetaDataSnapshot == SnapshotAny)
            {
                appendOnlyMetaDataSnapshot = GetTransactionSnapshot();
            }

            node->ss_currentScanDesc_aocs =
                aocs_beginscan(currentRelation,
                               node->ss.ps.state->es_snapshot,
                               appendOnlyMetaDataSnapshot,
                               NULL,
                               node->ss_aocs_proj);
*/
            elog(ERROR, "aoco table with plcontaienr scan not implemented.");
        }
        else
        {
            node->ss_currentScanDesc_heap = heap_beginscan(currentRelation,
                                                           estate->es_snapshot,
                                                           0, NULL);
        }
    }

    /*
     * get the next tuple from the table
     */
    if (node->ss_currentScanDesc_ao)
    {
        //appendonly_getnext(node->ss_currentScanDesc_ao, direction, slot);
        elog(ERROR, "ao table with plcontaienr scan not implemented.");
    }
    else if (node->ss_currentScanDesc_aocs)
    {
        //aocs_getnext(node->ss_currentScanDesc_aocs, direction, slot);
        elog(ERROR, "aoco table with plcontaienr scan not implemented.");
    }
    else
    {
        HeapScanDesc scandesc = node->ss_currentScanDesc_heap;

        tuple = heap_getnext(scandesc, direction);

        /*
         * save the tuple and the buffer returned to us by the access methods in
         * our scan tuple slot and return the slot.  Note: we pass 'false' because
         * tuples returned by heap_getnext() are pointers onto disk pages and were
         * not created with palloc() and so should not be pfree()'d.  Note also
         * that ExecStoreTuple will increment the refcount of the buffer; the
         * refcount will not be dropped until the tuple table slot is cleared.
         */
        if (tuple)
            ExecStoreHeapTuple(tuple,   /* tuple to store */
                           slot,    /* slot to store in */
                           scandesc->rs_cbuf,       /* buffer associated with this
                                                     * tuple */
                           false);  /* don't pfree this pointer */
        else
            ExecClearTuple(slot);
    }

    return slot;

}

static bool
SeqRecheck(SeqScanState *node, TupleTableSlot *slot)
{
    /*
     * Note that unlike IndexScan, SeqScan never use keys in heap_beginscan
     * (and this is very bad) - so, here we do not check are keys ok or not.
     */
    return true;
}

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

    PlcScanState *pss;
    CustomScan  *cscan;
    SeqScan     *node;

    /* clear state initialized in ExecInitCustomScan */
    //ClearCustomScanState(css);

    cscan = (CustomScan *)css->ss.ps.plan;
    node = (SeqScan *)linitial(cscan->custom_plans);

    pss = (PlcScanState*)css;
    pss->batch_num = 0;
    pss->batch_size = 0;
    pss->batch_status = PLC_BATCH_UNSTART;
    pss->cur_batch_scan_num = 0;
    pss->scanFinish = false;


    pss->seqstate = ExecInitSeqScan(node, estate, eflags);

    pss->css.ss.ps.ps_ResultTupleSlot = pss->seqstate->ss.ps.ps_ResultTupleSlot;

    TupleDesc   tupDesc = RelationGetDescr(pss->seqstate->ss.ss_currentRelation);

    for (int i=0;i<PLC_BATCH_SIZE;i++)
    {
        pss->batch[i] = MakeSingleTupleTableSlot(tupDesc); 
    }

    g_PlcScanState = pss;
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

    elog(ERROR, "plcontainer rescan not implemented yet.");
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

    return PlcExecScan((ScanState *) node,
                    (ExecScanAccessMtd) SeqNext,
                    (ExecScanRecheckMtd) SeqRecheck);
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

    PlcScanState *pss = (PlcScanState*)node;

    g_PlcScanState = NULL;

    for (int i=0;i<PLC_BATCH_SIZE;i++)
    {
        ExecDropSingleTupleTableSlot(pss->batch[i]);
    }

    return ExecEndSeqScan(pss->seqstate);;
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
