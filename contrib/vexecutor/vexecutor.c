/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include "vexecutor.h"
#include "utils/guc.h"
#include "vcheck.h"
#include "miscadmin.h"
#include "tuplebatch.h"
#include "executor/nodeTableScan.h"
#include "catalog/catquery.h"
#include "cdb/cdbvars.h"
#include "execVScan.h"

PG_MODULE_MAGIC;
int BATCHSIZE = 1024;
static int MINBATCHSIZE = 1;
static int MAXBATCHSIZE = 4096;
/*
 * hook function
 */
static PlanState* VExecInitNode(PlanState *node,EState *eState,int eflags,MemoryAccount* ptr);
static TupleTableSlot* VExecProcNode(PlanState *node);
static bool VExecEndNode(PlanState *node);
static Oid GetNType(Oid vtype);
extern int currentSliceId;

/*
 * _PG_init is called when the module is loaded. In this function we save the
 * previous utility hook, and then install our hook to pre-intercept calls to
 * the copy command.
 */
void
_PG_init(void)
{
	elog(DEBUG3, "PG INIT VEXECTOR");
	vmthd.CheckPlanVectorized_Hook = CheckAndReplacePlanVectorized;
	vmthd.ExecInitNode_Hook = VExecInitNode;
	vmthd.ExecProcNode_Hook = VExecProcNode;
	vmthd.ExecEndNode_Hook = VExecEndNode;
	vmthd.GetNType = GetNType;
	DefineCustomBoolVariable("vectorized_executor_enable",
	                         gettext_noop("enable vectorized executor"),
	                         NULL,
	                         &vmthd.vectorized_executor_enable,
	                         PGC_USERSET,
	                         NULL,NULL);

    DefineCustomIntVariable("vectorized_batch_size",
							gettext_noop("set vectorized execution batch size"),
                            NULL,
                            &BATCHSIZE,
                            MINBATCHSIZE,MAXBATCHSIZE,
							PGC_USERSET,
							NULL,NULL);
}

/*
 * _PG_fini is called when the module is unloaded. This function uninstalls the
 * extension's hooks.
 */
void
_PG_fini(void)
{
	elog(DEBUG3, "PG FINI VEXECTOR");
	vmthd.CheckPlanVectorized_Hook = NULL;
	vmthd.ExecInitNode_Hook = NULL;
	vmthd.ExecProcNode_Hook = NULL;
	vmthd.ExecEndNode_Hook = NULL;
}

/*
 *
 * backportTupleDescriptor
 *			Backport the TupleDesc information to normal type. Due to Vectorized type unrecognizable in normal execution node,
 *			the TupleDesc structure should backport metadata to normal type before we do V->N result pop up.
 */
static void backportTupleDescriptor(PlanState* ps,TupleDesc td)
{
	cqContext *pcqCtx;
	HeapTuple tuple;
	Oid oidtypeid;
	Form_pg_type  typeForm;
	for(int i = 0;i < td->natts; ++i)
	{
		oidtypeid = GetVFunc(td->attrs[i]->atttypid)->ntype;

		pcqCtx = caql_beginscan(
				NULL,
				cql("SELECT * FROM pg_type "
							" WHERE oid = :1 ",
					ObjectIdGetDatum(oidtypeid)));
		tuple = caql_getnext(pcqCtx);

		if (!HeapTupleIsValid(tuple))
			elog(ERROR, "cache lookup failed for type %u", oidtypeid);
		typeForm = (Form_pg_type) GETSTRUCT(tuple);

		td->attrs[i]->atttypid = oidtypeid;
		td->attrs[i]->attlen = typeForm->typlen;
		td->attrs[i]->attbyval = typeForm->typbyval;
		td->attrs[i]->attalign = typeForm->typalign;
		td->attrs[i]->attstorage = typeForm->typstorage;

		caql_endscan(pcqCtx);
	}

	ExecAssignResultType(ps,td);
}

static PlanState* VExecInitNode(PlanState *node,EState *eState,int eflags,MemoryAccount* curMemoryAccount)
{
	Plan *plan = node->plan;
	PlanState *subState = NULL;
	VectorizedState *vstate = (VectorizedState*)palloc0(sizeof(VectorizedState));

	elog(DEBUG3, "PG VEXECINIT NODE");

	if(NULL == node)
		return node;

	/* set state */
	node->vectorized = (void*)vstate;

	vstate->vectorized = plan->vectorized;

	/* set the parent state of son */
	if(innerPlanState(node))
	{
		subState = innerPlanState(node);
		Assert(NULL != subState);

		((VectorizedState*)(subState->vectorized))->parent = node;
	}
	if(outerPlanState(node))
	{
		subState = outerPlanState(node);
		Assert(NULL != subState);

		((VectorizedState*)(subState->vectorized))->parent = node;
	}

	if(Gp_role != GP_ROLE_DISPATCH)
	{
		switch (nodeTag(node) )
		{
			case T_AppendOnlyScan:
			case T_ParquetScan:
			case T_TableScanState:
				START_MEMORY_ACCOUNT(curMemoryAccount);
					{
						TupleDesc td = ((TableScanState *)node)->ss.ss_ScanTupleSlot->tts_tupleDescriptor;
						((TableScanState *)node)->ss.ss_ScanTupleSlot->PRIVATE_tb = PointerGetDatum(tbGenerate(td->natts,BATCHSIZE));
						node->ps_ResultTupleSlot->PRIVATE_tb = PointerGetDatum(tbGenerate(td->natts,BATCHSIZE));
						/* if V->N */
						backportTupleDescriptor(node,node->ps_ResultTupleSlot->tts_tupleDescriptor);
					}
						END_MEMORY_ACCOUNT();
				break;
			default:
				((VectorizedState *)node->vectorized)->vectorized = false;
				break;
		}
	}

	return node;
}
static TupleTableSlot* VExecProcNode(PlanState *node)
{
    TupleTableSlot* result = NULL;
    switch(nodeTag(node))
    {
        case T_ParquetScanState:
        case T_AppendOnlyScanState:
        case T_TableScanState:
            result = ExecTableVScan((TableScanState*)node);
            break;
        default:
            break;
    }
    return result;
}

static bool VExecEndNode(PlanState *node)
{
    if(Gp_role == GP_ROLE_DISPATCH)
		return false;

	elog(DEBUG3, "PG VEXEC END NODE");
	bool ret = false;
	switch (nodeTag(node))
	{
		case T_AppendOnlyScanState:
		case T_ParquetScanState:
            tbDestroy((TupleBatch *) (&node->ps_ResultTupleSlot->PRIVATE_tb));
            tbDestroy((TupleBatch *) (&((TableScanState *) node)->ss.ss_ScanTupleSlot->PRIVATE_tb));
			ret = true;
			break;
		case T_TableScanState:
			ExecEndTableScan((TableScanState *)node);
			ret = true;
			break;
		default:
			break;
	}
	return ret;
}

/* check if there is an vectorized execution operator */
bool
HasVecExecOprator(NodeTag tag)
{
	bool result = false;

	switch(tag)
	{
	case T_AppendOnlyScan:
	case T_ParquetScan:
		result = true;
		break;
	default:
		result = false;
		break;
	}

	return result;
}

static Oid GetNType(Oid vtype)
{
	const vFuncMap* vf = GetVFunc(vtype);
	return vf == NULL ? InvalidOid : vf->ntype;
}