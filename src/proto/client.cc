#include "client.h"
#include "proto_utils.h"

PLContainerClient::PLContainerClient(std::shared_ptr<grpc::Channel> channel) {
    this->stub_ = PLContainer::NewStub(channel);
}

PLContainerClient::PLContainerClient(const plcContext *ctx) {
    this->ctx = ctx;
    std::string uds(ctx->service_address);
    uds = "unix://" + uds;
    this->stub_ = PLContainer::NewStub(grpc::CreateChannel(uds, grpc::InsecureChannelCredentials()));
}

PLContainerClient::PLContainerClient() {
    this->stub_ = NULL;
    this->ctx = NULL;
}

void PLContainerClient::Init(const plcContext *ctx) {
    this->ctx = ctx;
    std::string uds(ctx->service_address);
    uds = "unix://" + uds;
    this->stub_ = PLContainer::NewStub(grpc::CreateChannel(uds, grpc::InsecureChannelCredentials()));
}

void PLContainerClient::FunctionCall(const CallRequest &request, CallResponse &response) {
    int client_connection_timeout = 5;
    std::chrono::system_clock::time_point deadline;
    grpc::Status status; 
    while (true) {
        CHECK_FOR_INTERRUPTS();

        grpc::ClientContext context;
        deadline = std::chrono::system_clock::now() + std::chrono::seconds(client_connection_timeout);
        context.set_deadline(deadline);
        context.set_wait_for_ready(true);

        plc_elog(DEBUG1, "function call request:%s", request.DebugString().c_str());
        status = stub_->FunctionCall(&context, request, &response);
        plc_elog(DEBUG1, "function call response:%s", response.DebugString().c_str());
        if (!status.ok()) {
            if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
                plc_elog(LOG, "plcontainer functioncall timeout");
            } else {
                plc_elog(ERROR, "plcontainer function call RPC failed., error:%s", status.error_message().c_str());
            }
            continue;
        } else if (response.has_exception()) {
/*
            Error *error = response.mutable_exception();
            plc_elog(ERROR, "plcontainer function call failed. error:%s stacktrace:%s", error->message().c_str(), error->stacktrace().c_str());
*/
            // fake setof
            response.set_runtimetype(R);
            PlcValue *value = NULL;
            if (response.results_size() == 0) {
                value = response.add_results();
            } else {
                value = response.mutable_results(0);
            }
            value->set_type(SETOF);
            value->set_name("plc_value_name");
            SetOfData *setof = value->mutable_setofvalue();

            setof->set_name("setof_name");
            setof->add_columnnames("a");
            setof->add_columnnames("b");
            setof->add_columnnames("c");
            setof->add_columntypes(INT);
            setof->add_columntypes(INT);
            setof->add_columntypes(INT);
           
            for (int i=1;i<=3;i++) { 
                CompositeData *cd = setof->add_rowvalues();
                cd->set_name("fake_udt");    
         
                ScalarData *sd = cd->add_values();
                sd->set_type(INT);
                sd->set_name("a");
                sd->set_isnull(false);
                sd->set_intvalue(100 * i);

                sd = cd->add_values();
                sd->set_type(INT);
                sd->set_name("b");
                sd->set_isnull(false);
                sd->set_intvalue(200 * i);

                sd = cd->add_values();
                sd->set_type(INT);
                sd->set_name("c");
                sd->set_isnull(false);
                sd->set_intvalue(300 * i);
            }
            break;
        } else {
            // success
            break;
        }
    }
    plc_elog(DEBUG1, "PLContainerClient function call finished with status %d", status.error_code());
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, ScalarData &arg) {
    PLContainerProtoUtils::SetScalarValue(arg,
                        proc->argnames[argIdx],
                        fcinfo->argnull[argIdx],
                        &proc->args[argIdx],
                        fcinfo->argnull[argIdx] ? NULL : proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]));
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, ArrayData &arg) {
    if (proc->argnames[argIdx]) {
        arg.set_name(proc->argnames[argIdx]);
    } else {
        arg.set_name("");
    }
     
    if (!fcinfo->argnull[argIdx]) {
        char *argvalue = proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]);
        int size = *(int *)argvalue;
        if (!arg.ParseFromArray(argvalue+sizeof(int), size)) {
            plc_elog(ERROR, "array outfunc parse failed");
        }
    }

    plc_elog(DEBUG1, "array data parse result:%s", arg.DebugString().c_str());
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, CompositeData &arg) {
    if (proc->argnames[argIdx]) {
        arg.set_name(proc->argnames[argIdx]);
    } else {
        arg.set_name("");
    }
     
    if (!fcinfo->argnull[argIdx]) {
        char *argvalue = proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]);
        int size = *(int *)argvalue;
        if (!arg.ParseFromArray(argvalue+sizeof(int), size)) {
            plc_elog(ERROR, "composited outfunc parse failed");
        }
    }

    plc_elog(DEBUG1, "composite data parse result:%s", arg.DebugString().c_str());
}

void PLContainerClient::initCallRequestArgument(const FunctionCallInfo fcinfo, const plcProcInfo *proc, int argIdx, SetOfData &arg) {
    if (proc->argnames[argIdx]) {
        arg.set_name(proc->argnames[argIdx]);
    } else {
        arg.set_name("");
    }

    if (!fcinfo->argnull[argIdx]) {
        char *argvalue = proc->args[argIdx].outfunc(fcinfo->arg[argIdx], &proc->args[argIdx]);
        int size = *(int *)argvalue;
        if (!arg.ParseFromArray(argvalue+sizeof(int), size)) {
            plc_elog(ERROR, "setof outfunc parse failed");
        }
    }

    plc_elog(DEBUG1, "setof data parse result:%s", arg.DebugString().c_str());
}

void PLContainerClient::InitCallRequest(const FunctionCallInfo fcinfo, const plcProcInfo *proc, PlcRuntimeType type, CallRequest &request) {
    plc_elog(DEBUG1, "fcinfo is :%s", PLContainerClient::functionCallInfoToStr(fcinfo).c_str());
    plc_elog(DEBUG1, "proc is %s", PLContainerClient::procInfoToStr(proc).c_str());

    request.set_runtimetype(type);
    request.set_objectid(proc->funcOid);
    request.set_haschanged(proc->hasChanged);
    request.mutable_proc()->set_src(proc->src);
    request.mutable_proc()->set_name(proc->name);
    request.set_loglevel(log_min_messages);
    PLContainerClient::setFunctionReturnType(request.mutable_rettype(), &proc->result);
    if (GetDatabaseEncoding() == PG_SQL_ASCII) {
        request.set_serverenc("ascii");
    } else {
        request.set_serverenc(GetDatabaseEncodingName());            
    }

    for (int i=0;i<proc->nargs;i++) {
        PlcValue *arg = request.add_args();
        arg->set_type(PLContainerProtoUtils::GetDataType(&proc->args[i]));
        if (proc->argnames[i]) {
            arg->set_name(proc->argnames[i]);
        } else {
            arg->set_name("");
        }
        switch (arg->type()) {
        case LOGICAL:
        case INT:
        case REAL:
        case TEXT:
        case BYTEA:
            PLContainerClient::initCallRequestArgument(fcinfo, proc, i, *arg->mutable_scalarvalue());
            break;
        case ARRAY:
            PLContainerClient::initCallRequestArgument(fcinfo, proc, i, *arg->mutable_arrayvalue());
            break;
        case COMPOSITE:
            PLContainerClient::initCallRequestArgument(fcinfo, proc, i, *arg->mutable_compositevalue());
            break;
        case SETOF:
            PLContainerClient::initCallRequestArgument(fcinfo, proc, i, *arg->mutable_setofvalue());
            break;
        default:
            plc_elog(ERROR, "invalid data type %d in argument %d in InitCallRequest", arg->type(), i);
        }
    }
}

void PLContainerClient::setFunctionReturnType(::plcontainer::ReturnType* rettype, const plcTypeInfo *type) {
    PlcDataType rt = PLContainerProtoUtils::GetDataType(type);
    if (rt == SETOF) {
        plc_elog(ERROR, "setof return type is not implemented.");
    } 
    rettype->set_type(rt);
    if (rt == ARRAY || rt == COMPOSITE || rt == SETOF) {
        const plcTypeInfo *t = (rt == SETOF ? &type->subTypes[0] : type);
        for (int i=0; i<t->nSubTypes; i++) {
            rettype->add_subtypes(PLContainerProtoUtils::GetDataType(&t->subTypes[i]));
        }
    } else {
        // no subtype for scalar return type
    }
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ScalarData &response) {
    Datum retresult = (Datum)0;

    if (response.isnull()) {
        return retresult;
    }

    char *buffer = NULL;
    switch (proc->result.type) {
    case PLC_DATA_INT1:
        buffer = (char *)palloc(1); 
        *(int8_t *)buffer = response.logicalvalue();
        break;
    case PLC_DATA_INT2:
        buffer = (char *)palloc(2); 
        *(int16_t *)buffer = response.intvalue();
        break;
    case PLC_DATA_INT4:
        buffer = (char *)palloc(4); 
        *(int32_t *)buffer = response.intvalue();
        break;
    case PLC_DATA_INT8:
        buffer = (char *)palloc(8); 
        *(int64_t *)buffer = response.realvalue();
        break;
    case PLC_DATA_FLOAT4:
        buffer = (char *)palloc(4);
        *(float *)buffer = response.realvalue();
        break;
    case PLC_DATA_FLOAT8:
        buffer = (char *)palloc(8);
        *(double *)buffer = response.realvalue();
        break;
    case PLC_DATA_TEXT:
        buffer = (char *)palloc(response.stringvalue().size()+1);
        strncpy(buffer, response.stringvalue().c_str(), response.stringvalue().size()+1);
        break;
    case PLC_DATA_BYTEA:
        buffer = (char *)palloc(response.byteavalue().size()+sizeof(int32_t));
        *(int32_t *)buffer = response.byteavalue().size();
        memcpy(buffer+sizeof(int32_t), response.byteavalue().data(), response.byteavalue().size());
        break;
    default:
        plc_elog(ERROR, "unknown scalar type:%d", proc->result.type);
    }
       
    if (buffer) { 
        fcinfo->isnull = false;
        retresult = proc->result.infunc(buffer, &proc->result);
        pfree(buffer);
    }
    return retresult;
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const ArrayData &response) {
    if (response.values_size() == 0) {
        return (Datum)0;
    } else {
        fcinfo->isnull = false;
        return proc->result.infunc((char *)&response, &proc->result);
    }
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CompositeData &response) {
    if (response.values_size() == 0) {
        return (Datum)0;
    } else {
        fcinfo->isnull = false;
        return proc->result.infunc((char *)&response, &proc->result);
    }
}

Datum PLContainerClient::getCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const SetOfData &response, int result_index) {
    if (response.rowvalues_size() == 0) {
        return (Datum)0;
    } else {
        fcinfo->isnull = false;
        return proc->result.infunc((char *)&response.rowvalues(result_index), &proc->result);
    }
}

Datum PLContainerClient::GetCallResponseAsDatum(const FunctionCallInfo fcinfo, plcProcInfo *proc, const CallResponse &response) {
    Datum retresult = (Datum) 0;
    if (response.results_size() != 1) {
        plc_elog(ERROR, "currently, only one result set is supported");
        return retresult; 
    }

    const PlcValue &result = response.results(0);
    switch (result.type()) {
    case LOGICAL:
    case INT:
    case REAL:
    case TEXT:
    case BYTEA:
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.scalarvalue());
    case ARRAY:
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.arrayvalue());
    case COMPOSITE:
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.compositevalue());
    case SETOF:
        return PLContainerClient::getCallResponseAsDatum(fcinfo, proc, result.setofvalue(), response.result_rows());
    default:
        plc_elog(ERROR, "uninvalid data type %d in ProtoToMessage", result.type());
    }
    
    return retresult;
}

std::string PLContainerClient::functionCallInfoToStr(const FunctionCallInfo fcinfo) {
    char result[1024];
    std::string argnull;
    for (int i=0;i<fcinfo->nargs;i++) {
        if (fcinfo->argnull[i]) {
            argnull += "1,";
        } else {
            argnull += "0,";
        }
    }
    argnull = argnull.substr(0, argnull.length()-1);

    snprintf(result, sizeof(result), 
                    "fcinfo->isnull:%d "
                    "fcinfo->nargs:%d "
                    "fcinfo->argnull:[%s]",
                    fcinfo->isnull,
                    fcinfo->nargs,
                    argnull.c_str());
    return std::string(result);
}

std::string PLContainerClient::procInfoToStr(const plcProcInfo *proc) {
    char result[4096];

    std::string argnames;
    std::string args;
    for (int i=0;i<proc->nargs;i++) {
        if (proc->argnames[i]) {
            argnames += std::string(proc->argnames[i]) + ",";
        } else {
            argnames += ",";
        }
        args += PLContainerClient::typeInfoToStr(&proc->args[i]) + ",";
    }
    argnames = argnames.substr(0, argnames.length()-1); 
    args = args.substr(0, args.length()-1); 
 
    snprintf(result, sizeof(result), 
                    "proname:%s "
                    "plname:%s "
                    "fn_readonly:%d "
                    "result:%s "
                    "src:%s "
                    "argnames:[%s] "
                    "args:[%s] "
                    "nargs:%d "
                    "name:%s "
                    "hasChanged:%d "
                    "retset:%d "
                    "funcOid:%d",
                    proc->proname,
                    proc->pyname,
                    proc->fn_readonly,
                    PLContainerClient::typeInfoToStr(&proc->result).c_str(),
                    proc->src,
                    argnames.c_str(),
                    args.c_str(),
                    proc->nargs,
                    proc->name,
                    proc->hasChanged,
                    proc->retset,
                    proc->funcOid);
    return std::string(result);
}

std::string PLContainerClient::typeInfoToStr(const plcTypeInfo *type) {
    char result[2048];

    snprintf(result, sizeof(result),
                    "type:%d "
                    "nSubTypes:%d "
                    "is_rowtype:%d "
                    "is_record:%d "
                    "attisdropped:%d "
                    "typ_relid:%d "
                    "typeName:%s ",
                    type->type,
                    type->nSubTypes,
                    type->is_rowtype,
                    type->is_record,
                    type->attisdropped,
                    type->typ_relid,
                    type->typeName);
    for (int i=0;i<type->nSubTypes;i++) {

        snprintf(result+strlen(result), sizeof(result) - strlen(result) - 2, "subtype_%d:%s", i,
                    PLContainerClient::typeInfoToStr(&type->subTypes[i]).c_str());
    }
    return std::string(result);
}

Datum plcontainer_function_handler(FunctionCallInfo fcinfo, plcProcInfo *proc, MemoryContext function_cxt) {
    Datum datumreturn;
    MemoryContext volatile      oldcontext = CurrentMemoryContext;
    FuncCallContext * volatile  funcctx = NULL;
    bool     volatile               bFirstTimeCall = false;
    char *runtime_id;
    plcContext *ctx = NULL;
    CallRequest     request;
    CallResponse    * volatile  response = NULL; 
    PLContainerClient client;

    PG_TRY();
    {
        // 1. initialize function handler context, both return set or not
        plc_elog(DEBUG1, "fcinfo->flinfo->fn_retset: %d", fcinfo->flinfo->fn_retset);

        if (fcinfo->flinfo->fn_retset) {
            /* First Call setup */
            if (SRF_IS_FIRSTCALL())
            {
                funcctx = SRF_FIRSTCALL_INIT();
                bFirstTimeCall = true;

                plc_elog(DEBUG1, "The funcctx pointer returned by SRF_FIRSTCALL_INIT() is: %p", funcctx);
            }

            /* Every call setup */
            funcctx = SRF_PERCALL_SETUP();
            plc_elog(DEBUG1, "The funcctx pointer returned by SRF_PERCALL_SETUP() is: %p", funcctx);

            Assert(funcctx != NULL);
            /* SRF uses multi_call_memory_ctx context shared between function calls,
             * since EXPR etc. context will be cleared after one of the SRF calls.
             * Note that plpython doesn't need it, because it doesn't use palloc to store
             * the SRF result.
             */
            oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
        } else {
            oldcontext = MemoryContextSwitchTo(function_cxt);
        }

        if (!fcinfo->flinfo->fn_retset || bFirstTimeCall) {
            runtime_id = parse_container_meta(proc->src);
            ctx = get_container_context(runtime_id);
            /*
             * TODO will be reuse client channel if possible
             */
            client.Init(ctx);
            client.InitCallRequest(fcinfo, proc, R, request);
            
            plcContextBeginStage(ctx, "R_function_call", NULL);
            response = new CallResponse;
            client.FunctionCall(request, *response);
            response->set_result_rows(0);
            plcContextEndStage(ctx, "R_function_call",
                    PLC_CONTEXT_STAGE_SUCCESS,
                    "[REQUEST]:%s, [RESPONSE]:%s", request.DebugString().c_str(), response->DebugString().c_str());
            plcContextLogging(LOG, ctx);
            bFirstTimeCall = false;
        }

        if (fcinfo->flinfo->fn_retset) {
            ReturnSetInfo *rsi = (ReturnSetInfo *) fcinfo->resultinfo;

            if (funcctx->user_fctx == NULL) {
                plc_elog(DEBUG1, "first time call, preparing the result set...");

                /* first time -- do checks and setup */
                if (!rsi || !IsA(rsi, ReturnSetInfo)
                        || (rsi->allowedModes & SFRM_ValuePerCall) == 0) {
                    ereport(ERROR,
                            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg(
                                    "unsupported set function return mode"), errdetail(
                                    "PL/Python set-returning functions only support returning only value per call.")));
                }
                rsi->returnMode = SFRM_ValuePerCall;

                funcctx->user_fctx = (void *) response;

                if (funcctx->user_fctx == NULL)
                    ereport(ERROR,
                            (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg(
                                    "returned object cannot be iterated"), errdetail(
                                    "PL/Python set-returning functions must return an iterable object.")));
            }        

            response = (CallResponse *) funcctx->user_fctx;
            const SetOfData &setof = response->results(0).setofvalue();
            
            if (response->result_rows() < setof.rowvalues_size()) {
                rsi->isDone = ExprMultipleResult;
            } else {
                rsi->isDone = ExprEndResult;
            }

            if (rsi->isDone == ExprEndResult) {
                delete response;
                MemoryContextSwitchTo(oldcontext);

                funcctx->user_fctx = NULL;

                SRF_RETURN_DONE(funcctx);
            }
        }

        /* Process the result message from client */
        datumreturn = client.GetCallResponseAsDatum(fcinfo, proc, *response);
        response->set_result_rows(response->result_rows() + 1);
        MemoryContextSwitchTo(oldcontext);
    }
    PG_CATCH();
    {
        /*
         * If there was an error the iterator might have not been exhausted
         * yet. Set it to NULL so the next invocation of the function will
         * start the iteration again.
         */
        if (fcinfo->flinfo->fn_retset && funcctx->user_fctx != NULL) {
            funcctx->user_fctx = NULL;
        }
        if(response) {
            delete response;
        }
        MemoryContextSwitchTo(oldcontext);
        PG_RE_THROW();
    }
    PG_END_TRY();
   
    if (fcinfo->flinfo->fn_retset) {
        SRF_RETURN_NEXT(funcctx, datumreturn);
    } else {
        delete response;
    }
 
    return datumreturn;
}

int get_new_container_from_coordinator(const char *runtime_id, plcContext *ctx) {
    (void) runtime_id;
    StartContainerRequest   request;
    StartContainerResponse  response;

    plcContextBeginStage(ctx, "request_coordinator_for_container", NULL);

    std::string server_addr = get_coordinator_address();
    PLCoordinatorClient     client(grpc::CreateChannel(
      "unix://"+server_addr, grpc::InsecureChannelCredentials()));

    request.set_runtime_id(runtime_id);
    request.set_qe_pid(getpid());
    request.set_session_id(gp_session_id);
    request.set_command_count(gp_command_count);
    client.StartContainer(request, response);

    plcContextEndStage(ctx, "request_coordinator_for_container",
                    response.status() == 0 ? PLC_CONTEXT_STAGE_SUCCESS : PLC_CONTEXT_STAGE_FAIL,
                    "[REQUEST]:%s, [RESPONSE]:%s", request.DebugString().c_str(), response.DebugString().c_str());

    if (response.status() != 0) {
        return -1;
    }
    ctx->service_address = plc_top_strdup(response.container_address().c_str());
    ctx->container_id = plc_top_strdup(response.container_id().c_str());
    return 0;
}

PLCoordinatorClient::PLCoordinatorClient(std::shared_ptr<grpc::Channel> channel) {
    this->stub_ = PLCoordinator::NewStub(channel);
}

void PLCoordinatorClient::StartContainer(const StartContainerRequest &request, StartContainerResponse &response) {
    grpc::ClientContext context;
    plc_elog(DEBUG1, "StartContainer request:%s", request.DebugString().c_str());
    grpc::Status status = stub_->StartContainer(&context, request, &response);
    if (!status.ok()) {
        response.set_status(1);
        plc_elog(ERROR, "StartContainer RPC failed., error:%s", status.error_message().c_str());
    }
    plc_elog(DEBUG1, "StartContainer response:%s", response.DebugString().c_str());
    plc_elog(DEBUG1, "StartContainer finished with status %d", status.error_code());
}

void PLCoordinatorClient::StopContainer(const StopContainerRequest &request, StopContainerResponse &response) {
    grpc::ClientContext context;
    plc_elog(DEBUG1, "StopContainer request:%s", request.DebugString().c_str());
    grpc::Status status = stub_->StopContainer(&context, request, &response);
    if (!status.ok()) {
        plc_elog(ERROR, "StopContainer RPC failed., error:%s", status.error_message().c_str());
        response.set_status(1);
    }
    plc_elog(DEBUG2, "StopContainer response:%s", response.DebugString().c_str());
    plc_elog(DEBUG1, "StopContainer finished with status %d", status.error_code());
}
