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
PlcExecScan(PlcScanState *vss,
            ExecScanAccessMtd accessMtd,  /* function returning a tuple */
            ExecScanRecheckMtd recheckMtd)
{
    ScanState *node = vss->seqstate;

    ExprContext *econtext;
    List       *qual;
    ProjectionInfo *projInfo;

    /*
     * Fetch data from node
     */
    qual = node->ps.qual;
    projInfo = node->ps.ps_ProjInfo;
    econtext = node->ps.ps_ExprContext;

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

    for (;;)
    {
        TupleTableSlot *slot;

        CHECK_FOR_INTERRUPTS();

        if (QueryFinishPending)
            return NULL;

        slot = ExecScanFetch(node, accessMtd, recheckMtd);

        /*
         * if the slot returned by the accessMtd contains NULL, then it means
         * there is nothing more to scan so we just return an empty slot,
         * being careful to use the projection result slot so it has correct
         * tupleDesc.
         */
        if (TupIsNull(slot))
        {
            if (projInfo)
                return ExecClearTuple(projInfo->pi_slot);
            else
                return slot;
        }

        /*
         * place the current tuple into the expr context
         */
        econtext->ecxt_scantuple = slot;

        /*
         * check that the current tuple satisfies the qual-clause
         *
         * check for non-nil qual here to avoid a function call to ExecQual()
         * when the qual is nil ... saves only a few cycles, but they add up
         * ...
         */
        if (!qual || ExecQual(qual, econtext, false))
        {
            /*
             * Found a satisfactory scan tuple.
             */
            if (projInfo)
            {
                /*
                 * Form a projection tuple, store it in the result tuple slot
                 * and return it.
                 */
                return ExecProject(projInfo, NULL);
            }
            else
            {
                /*
                 * Here, we aren't projecting, so just return scan tuple.
                 */
                return slot;
            }
        }
        else
            InstrCountFiltered1(node, 1);

        /*
         * Tuple fails qual, so free per-tuple memory and try again.
         */
        ResetExprContext(econtext);
    }
}
