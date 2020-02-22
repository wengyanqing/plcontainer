#include "postgres.h"
#include "access/htup.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "catalog/pg_proc.h"
#include "miscadmin.h"
#include "access/htup_details.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/var.h"
#include "parser/parse_oper.h"
#include "parser/parse_func.h"
#include "parser/parse_coerce.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/primnodes.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "utils/acl.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

#include "plcPlan.h"
#include "nodePlcscan.h"

typedef struct PlcPlanContext 
{
} PlcPlanContext;

static void mutate_plan_fields(Plan *newplan, Plan *oldplan, Node *(*mutator) (), void *context);
static Node* plan_tree_mutator(Node *node, Node *(*mutator) (), void *context);
static Node* PlcPlanMutator(Node *node, PlcPlanContext *ctx);

/*
 *  Check all the expressions if they have plcontainer function. 
 *  
 *  NOTE: if an expressions has plcontainer function, we return true and walker will be over
 */ 
Node*
PlcPlanMutator(Node *node, PlcPlanContext *ctx)
{
    if(NULL == node)
        return NULL;

    switch (nodeTag(node))
    {
    default:
        return plan_tree_mutator(node, PlcPlanMutator, ctx);
    }
}

Node *
plan_tree_mutator(Node *node,
                  Node *(*mutator) (),
                  void *context)
{
/*
 * The mutator has already decided not to modify the current node, but we
 * must call the mutator for any sub-nodes.
 */
#define FLATCOPY(newnode, node, nodetype)  \
    ( (newnode) = makeNode(nodetype), \
      memcpy((newnode), (node), sizeof(nodetype)) )

#define CHECKFLATCOPY(newnode, node, nodetype)  \
    ( AssertMacro(IsA((node), nodetype)), \
      (newnode) = makeNode(nodetype), \
      memcpy((newnode), (node), sizeof(nodetype)) )

#define MUTATE(newfield, oldfield, fieldtype)  \
        ( (newfield) = (fieldtype) mutator((Node *) (oldfield), context) )

#define PLANMUTATE(newplan, oldplan) \
        mutate_plan_fields((Plan*)(newplan), (Plan*)(oldplan), mutator, context)

/* This is just like  PLANMUTATE because Scan adds only scalar fields. */
#define SCANMUTATE(newplan, oldplan) \
        mutate_plan_fields((Plan*)(newplan), (Plan*)(oldplan), mutator, context)

#define JOINMUTATE(newplan, oldplan) \
        mutate_join_fields((Join*)(newplan), (Join*)(oldplan), mutator, context)

#define COPYARRAY(dest,src,lenfld,datfld) \
    do { \
        (dest)->lenfld = (src)->lenfld; \
        if ( (src)->lenfld > 0  && \
             (src)->datfld != NULL) \
        { \
            Size _size = ((src)->lenfld*sizeof(*((src)->datfld))); \
            (dest)->datfld = palloc(_size); \
            memcpy((dest)->datfld, (src)->datfld, _size); \
        } \
        else \
        { \
            (dest)->datfld = NULL; \
        } \
    } while (0)


    if (node == NULL)
        return NULL;

    /* Guard against stack overflow due to overly complex expressions */
    check_stack_depth();

    switch (nodeTag(node))
    {
    case T_SeqScan:
        {
            elog(NOTICE, "seqscan is replaced with custom scan");
            CustomScan  *cscan;
            SeqScan *plcscan;

            cscan = MakeCustomScanForSeqScan();
            FLATCOPY(plcscan, node, SeqScan);
            cscan->custom_plans = lappend(cscan->custom_plans, plcscan);

            SCANMUTATE(plcscan, node);
            return (Node *)cscan;
        }
        break;

    case T_Motion:
        {
            Motion     *motion = (Motion *) node;
            Motion     *newmotion;

            FLATCOPY(newmotion, motion, Motion);
            PLANMUTATE(newmotion, motion);
            MUTATE(newmotion->hashExprs, motion->hashExprs, List *);
            COPYARRAY(newmotion, motion, numSortCols, sortColIdx);
            COPYARRAY(newmotion, motion, numSortCols, sortOperators);
            COPYARRAY(newmotion, motion, numSortCols, nullsFirst);
            return (Node *) newmotion;
        }
        break;

    case T_Const:
        {
            Const      *oldnode = (Const *) node;
            Const      *newnode;

            FLATCOPY(newnode, oldnode, Const);
            return (Node *) newnode;
        }

    case T_Var:
    {
        Var        *var = (Var *)node;
        Var        *newnode;

        FLATCOPY(newnode, var, Var);
        return (Node *)newnode;
    }

    case T_OpExpr:
        {
            OpExpr     *expr = (OpExpr *)node;
            OpExpr     *newnode;

            FLATCOPY(newnode, expr, OpExpr);
            MUTATE(newnode->args, expr->args, List *);
            return (Node *)newnode;
        }

    case T_FuncExpr:
        {
            FuncExpr       *expr = (FuncExpr *)node;
            FuncExpr       *newnode;

            FLATCOPY(newnode, expr, FuncExpr);
            MUTATE(newnode->args, expr->args, List *);
            return (Node *)newnode;
        }

    case T_List:
        {
            /*
             * We assume the mutator isn't interested in the list nodes
             * per se, so just invoke it on each list element. NOTE: this
             * would fail badly on a list with integer elements!
             */
            List       *resultlist;
            ListCell   *temp;

            resultlist = NIL;
            foreach(temp, (List *) node)
            {
                resultlist = lappend(resultlist,
                                     mutator((Node *) lfirst(temp),
                                             context));
            }
            return (Node *) resultlist;
        }

    case T_TargetEntry:
        {
            TargetEntry *targetentry = (TargetEntry *) node;
            TargetEntry *newnode;

            FLATCOPY(newnode, targetentry, TargetEntry);
            MUTATE(newnode->expr, targetentry->expr, Expr *);
            return (Node *) newnode;
        }

    default:
        elog(ERROR, "node type %d:%s not supported", nodeTag(node), nodeToString(node));
/* 
    default:
        elog(ERROR, "node type %d:%s not supported", nodeTag(node), nodeToString(node));
        break;
*/
    }
}

/* Function mutate_plan_fields() is a subroutine for plan_tree_mutator().
 * It "hijacks" the macro MUTATE defined for use in that function, so don't
 * change the argument names "mutator" and "context" use in the macro
 * definition.
 * 
 */
static void
mutate_plan_fields(Plan *newplan, Plan *oldplan, Node *(*mutator) (), void *context)
{
    /*
     * Scalar fields startup_cost total_cost plan_rows plan_width nParamExec
     * need no mutation.
     */

    /* Node fields need mutation. */
    MUTATE(newplan->targetlist, oldplan->targetlist, List *);
    MUTATE(newplan->qual, oldplan->qual, List *);
    MUTATE(newplan->lefttree, oldplan->lefttree, Plan *);
    MUTATE(newplan->righttree, oldplan->righttree, Plan *);
    MUTATE(newplan->initPlan, oldplan->initPlan, List *);

    /* Bitmapsets aren't nodes but need to be copied to palloc'd space. */
    newplan->extParam = bms_copy(oldplan->extParam);
    newplan->allParam = bms_copy(oldplan->allParam);
}

/*
 *  Replace the plcontainer custom scan node 
 */
Plan*
ReplacePlanNodeWalker(Node *node)
{
    return (Plan *)plan_tree_mutator(node, PlcPlanMutator, NULL);
}

