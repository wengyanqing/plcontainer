#include "postgres.h"

#include "executor/executor.h"
#include "miscadmin.h"
#include "utils/memutils.h"

#include "executor.h"
#include "nodePlcscan.h"

/*
 *  * ExecScanFetch -- fetch next potential tuple
 *
 * This routine is concerned with substituting a test tuple if we are
 * inside an EvalPlanQual recheck.  If we aren't, just execute
 * the access method's next-tuple routine.
 */
static inline TupleTableSlot *
ExecScanFetch(ScanState *node,
              ExecScanAccessMtd accessMtd,
              ExecScanRecheckMtd recheckMtd)
{
    EState     *estate = node->ps.state;

    if (estate->es_epqTuple != NULL)
    {
        /*
         * We are inside an EvalPlanQual recheck.  Return the test tuple if
         * one is available, after rechecking any access-method-specific
         * conditions.
         */
        Index       scanrelid = ((Scan *) node->ps.plan)->scanrelid;

        if (scanrelid == 0)
        {
            TupleTableSlot *slot = node->ss_ScanTupleSlot;

            /*
             * This is a ForeignScan or CustomScan which has pushed down a
             * join to the remote side.  The recheck method is responsible not
             * only for rechecking the scan/join quals but also for storing
             * the correct tuple in the slot.
             */
            if (!(*recheckMtd) (node, slot))
                ExecClearTuple(slot);   /* would not be returned by scan */
            return slot;
        }
        else if (estate->es_epqTupleSet[scanrelid - 1])
        {
            TupleTableSlot *slot = node->ss_ScanTupleSlot;

            /* Return empty slot if we already returned a tuple */
            if (estate->es_epqScanDone[scanrelid - 1])
                return ExecClearTuple(slot);
            /* Else mark to remember that we shouldn't return more */
            estate->es_epqScanDone[scanrelid - 1] = true;

            /* Return empty slot if we haven't got a test tuple */
            if (estate->es_epqTuple[scanrelid - 1] == NULL)
                return ExecClearTuple(slot);

            /* Store test tuple in the plan node's scan slot */
            ExecStoreHeapTuple(estate->es_epqTuple[scanrelid - 1],
                           slot, InvalidBuffer, false);

            /* Check if it meets the access-method conditions */
            if (!(*recheckMtd) (node, slot))
                ExecClearTuple(slot);   /* would not be returned by scan */

            return slot;
        }
    }

    /*
     * Run the node-type-specific access method function to get the next tuple
     */
    return (*accessMtd) (node);
}





TupleTableSlot *
PlcExecScan(PlcScanState *pss,
            ExecScanAccessMtd accessMtd,  /* function returning a tuple */
            ExecScanRecheckMtd recheckMtd)
{
    ScanState *node = pss->seqstate;

    ExprContext *econtext;
    List       *qual;
    ProjectionInfo *projInfo;

    /*
     * Fetch data from node
     */
    qual = node->ps.qual;
    projInfo = node->ps.ps_ProjInfo;
    econtext = node->ps.ps_ExprContext;

    TupleDesc   tupDesc = RelationGetDescr(node->ss_currentRelation);

    /*
     * If we have neither a qual to check nor a projection to do, just skip
     * all the overhead and return the raw scan tuple.
     */
    if (!qual && !projInfo)
    {
        ResetExprContext(econtext);
        return ExecScanFetch(node, accessMtd, recheckMtd);
    }

    /*
     * Reset per-tuple memory context to free any expression evaluation
     * storage allocated in the previous tuple cycle.
     */
    ResetExprContext(econtext);

    /*
     * get a tuple from the access method.  Loop until we obtain a tuple that
     * passes the qualification.
     */
    TupleTableSlot *slot;

    while(true)
    {
        if (pss->batch_status == PLC_BATCH_SCAN_FINISH && pss->scanFinish)
        {
            elog(NOTICE, "plcontainer scan finish");
            return NULL;
        }

        if (pss->batch_status == PLC_BATCH_SCAN_FINISH)
        {
            for (int i=0;i<pss->batch_size;i++)
            {
                ExecDropSingleTupleTableSlot(pss->batch[i]);
            }

            pss->batch_size = 0;
            pss->cur_batch_scan_num = 0;
            pss->batch_status = PLC_BATCH_UNSTART;
            elog(NOTICE, "plcontainer scan, status:PLC_BATCH_SCAN_FINISH will to get next tuple batch");
        }
        else if (pss->batch_status == PLC_BATCH_SCAN_IN_PROCESS)
        {
            if (pss->batch_size == 0)
            {
                return NULL;
            }

            // batch ready, pop slot
            slot = pss->batch[pss->cur_batch_scan_num]; 
            pss->cur_batch_scan_num++;        
            elog(NOTICE, "plcontainer scan, status:PLC_BATCH_SCAN_IN_PROCESS tuple:%d/%d", pss->cur_batch_scan_num, pss->batch_size);

            econtext->ecxt_scantuple = slot;
            if (!qual || ExecQual(qual, econtext, false))
            {
                if (projInfo)
                {
                    slot = ExecProject(projInfo, NULL);
                    if (pss->cur_batch_scan_num == pss->batch_size)
                    {
                        pss->batch_status = PLC_BATCH_SCAN_FINISH;
                    }
                    elog(NOTICE, "plcontainer scan, status:PLC_BATCH_SCAN_IN_PROCESS scan:%d/%d", pss->cur_batch_scan_num, pss->batch_size);
                    return slot;
                }
                else
                {
                    return slot;
                }
            }
            else
                InstrCountFiltered1(node, 1);

            ResetExprContext(econtext);
        } 
        else if (pss->batch_status == PLC_BATCH_UNSTART)
        {
            elog(NOTICE, "plcontainer scan, status:PLC_BATCH_UNSTART");

            int batch_tuple_num = 0;
            for (;;) 
            {

                CHECK_FOR_INTERRUPTS();

                if (QueryFinishPending)
                    return NULL;

                slot = ExecScanFetch(node, accessMtd, recheckMtd);
                if (TupIsNull(slot))
                {
                    if (projInfo) 
                    {
                        ExecClearTuple(projInfo->pi_slot);
                    }
                    pss->batch_size = batch_tuple_num;
                    pss->batch_status = PLC_BATCH_FETCH_FINISH;
                    pss->scanFinish = true;
                    elog(NOTICE, "plcontainer scan, status:PLC_BATCH_FETCH_FINISH batch_size:%d", pss->batch_size);
                    break;
                }
                else
                { 
                    pss->batch[batch_tuple_num] = MakeSingleTupleTableSlot(tupDesc);
                    pss->batch[batch_tuple_num] = ExecCopySlot(pss->batch[batch_tuple_num], slot); 
                    batch_tuple_num++;
                    if (batch_tuple_num == PLC_BATCH_SIZE)
                    {
                        pss->batch_size = PLC_BATCH_SIZE;
                        pss->batch_status = PLC_BATCH_FETCH_FINISH;
                        elog(NOTICE, "plcontainer scan, status:PLC_BATCH_FETCH_FINISH batch_size:%d", pss->batch_size);
                        break;
                    }
                }
            }
            

        } 
        else if (pss->batch_status == PLC_BATCH_FETCH_FINISH) 
        {
            pss->cur_batch_scan_num = 0;
            pss->batch_status = PLC_BATCH_SCAN_IN_PROCESS; 
        }
        else
        {
            elog(ERROR, "error status for plcontainer scan");
        }
    }

    return slot;
}
