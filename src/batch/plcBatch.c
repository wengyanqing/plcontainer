#include "postgres.h"

#include "fmgr.h"
#include "optimizer/planner.h"
#include "executor/nodeCustom.h"
#include "utils/guc.h"

#include "access/relscan.h"
#include "executor/execdebug.h"
#include "executor/nodeSeqscan.h"
#include "utils/rel.h"

#include "nodes/extensible.h"
#include "executor/nodeCustom.h"
#include "utils/memutils.h"

// plcontainer headers
#include "nodePlcscan.h"
#include "plcPlan.h"

PG_MODULE_MAGIC;

static bool                 enable_plc_batch_mode;
static planner_hook_type    planner_hook_next;

static PlannedStmt *plc_batch_post_planner(Query *parse, int cursorOptions, ParamListInfo boundParams);

void    _PG_init(void);

static PlannedStmt *
plc_batch_post_planner(Query   *parse,
                    int     cursorOptions,
                    ParamListInfo   boundParams)
{
    elog(LOG, "plc_batch_post_planner begin"); 
    
    PlannedStmt *stmt;
    Plan        *savedPlanTree;
    List        *savedSubplan;

    if (planner_hook_next)
        stmt = planner_hook_next(parse, cursorOptions, boundParams);
    else
        stmt = standard_planner(parse, cursorOptions, boundParams);

    if (!enable_plc_batch_mode) {
        elog(LOG, "plc_batch_post_planner is not enabled");
        return stmt;
    }

    savedPlanTree = stmt->planTree;
    savedSubplan = stmt->subplans;

    PG_TRY();
    {
        List        *subplans = NULL;
        ListCell    *cell;

        stmt->planTree = ReplacePlanNodeWalker((Node *) stmt->planTree);

        foreach(cell, stmt->subplans)
        {
            Plan    *subplan = ReplacePlanNodeWalker((Node *)lfirst(cell));
            subplans = lappend(subplans, subplan);
        }
        stmt->subplans = subplans;

    }
    PG_CATCH();
    {
        /*
        ErrorData  *edata;
        edata = CopyErrorData();
        FlushErrorState();
        */

        // fallback
        elog(NOTICE, "query don't have plcontainer function");
        stmt->planTree = savedPlanTree;
        stmt->subplans = savedSubplan;
    }
    PG_END_TRY();

    elog(LOG, "plc_batch_post_planner finish"); 

    return stmt;
}

void
_PG_init(void)
{
    elog(LOG, "Initialize plcontainer batch extension");

    /* Register customscan node for plcontainer function scan */
    InitPlcScan();

    /* planner hook registration */
    planner_hook_next = planner_hook;
    planner_hook = plc_batch_post_planner;

    DefineCustomBoolVariable("enable_plc_batch_mode",
                             "Enables plcontainer batch execution.",
                             NULL,
                             &enable_plc_batch_mode,
                             false,
                             PGC_USERSET,
                             GUC_NOT_IN_SAMPLE,
                             NULL, NULL, NULL);
}
