#include "comm_channel.h"
#include "comm_utils.h"
#include "comm_connectivity.h"
#include "comm_server.h"
#include "config.h"
#include "message_protoutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static Plcontainer__RawData *rawdataToProto(plcType *type, rawdata *obj);
static int protoToRawdata(Plcontainer__RawData *proto, plcType *type, rawdata *obj); 
 
static Plcontainer__PlcType *plcTypeToProto(plcType *obj);
static int protoToPlcType(Plcontainer__PlcType *proto, plcType *obj); 

static Plcontainer__Argument *plcArgumentToProto(plcArgument *obj);
static int protoToPlcArgument(Plcontainer__Argument *proto, plcArgument *obj); 

static Plcontainer__RawData *rawdataToProto(plcType *type, rawdata *obj) {
    Plcontainer__RawData *ret = pmalloc(sizeof(Plcontainer__RawData));
    plcontainer__raw_data__init(ret);

    ret->isnull = obj->isnull;
    ret->value.data = (uint8_t *)(obj->value);
    switch (type->type) {
        case PLC_DATA_INT1:
            ret->value.len = 1;
            break;
        case PLC_DATA_INT2:
            ret->value.len = 2;
            break;
        case PLC_DATA_INT4:
            ret->value.len = 4;
            break;
        case PLC_DATA_INT8:
            ret->value.len = 8;
            break;
        case PLC_DATA_FLOAT4:
            ret->value.len = 4;
            break;
        case PLC_DATA_FLOAT8:
            ret->value.len = 8;
            break;
        case PLC_DATA_TEXT:
            ret->value.len = strlen(obj->value);
            break;
        case PLC_DATA_BYTEA:
            ret->value.len = *((int *)obj->value);
            ret->value.data = (uint8_t *)(obj->value + 4); 
            break;
        case PLC_DATA_ARRAY:
        case PLC_DATA_UDT:
            // TODO
            plc_elog(ERROR, "[TODO] unimplement type in rawdataToProto: %d", type->type);
            break;
        default:
            plc_elog(ERROR, "unsupported type: %d", type->type);
        break;
    }

    return ret; 
}

static int protoToRawdata(Plcontainer__RawData *proto, plcType *type, rawdata *obj) {
    obj->isnull = proto->isnull;
    switch (type->type) {
        case PLC_DATA_INT1:
        case PLC_DATA_INT2:
        case PLC_DATA_INT4:
        case PLC_DATA_INT8:
        case PLC_DATA_FLOAT4:
        case PLC_DATA_FLOAT8:
            obj->value = pmalloc(proto->value.len);
            memcpy(obj->value, proto->value.data, proto->value.len);
            break;
        case PLC_DATA_TEXT:
            obj->value = pmalloc(proto->value.len + 1);
            memcpy(obj->value, proto->value.data, proto->value.len);
            obj->value[proto->value.len] = '\0';
            break;
        case PLC_DATA_BYTEA:
            obj->value = pmalloc(proto->value.len + 4);
            memcpy(obj->value, &proto->value.len, 4);
            memcpy(obj->value+4, proto->value.data, proto->value.len);
            break;
        case PLC_DATA_ARRAY:
        case PLC_DATA_UDT:
            // TODO
            plc_elog(ERROR, "[TODO] unimplement type in protoToRawdata: %d", type->type);
            break;
        default:
            plc_elog(ERROR, "unsupported type: %d", type->type);
        break;
    }

    return 0; 
}

static Plcontainer__PlcType *plcTypeToProto(plcType *obj) {
    Plcontainer__PlcType *ret = pmalloc(sizeof(Plcontainer__PlcType));
    plcontainer__plc_type__init(ret);

    ret->type = obj->type;
    ret->typename_ = obj->typeName;
    if (obj->type == PLC_DATA_ARRAY || obj->type == PLC_DATA_UDT) {
        // TODO
        plc_elog(ERROR, "[TODO] unimplement type in plcTypeToProto: %d", obj->type);
    } else {
        ret->nsubtypes = 0;
        ret->subtypes = NULL;
    }

    return ret;
}

static int protoToPlcType(Plcontainer__PlcType *proto, plcType *obj) {
    obj->type = proto->type;
    obj->typeName = pstrdup(proto->typename_);
    if (proto->type == PLCONTAINER__DATA_TYPE__ARRAY || proto->type == PLCONTAINER__DATA_TYPE__UDT) {
        // TODO
        plc_elog(ERROR, "[TODO] unimplement type in protoToPlcType: %d", proto->type);
    } else {
        obj->nSubTypes = 0;
        obj->subTypes = NULL;
    }

    return 0;
}

static Plcontainer__Argument *plcArgumentToProto(plcArgument *obj) {
    Plcontainer__Argument *ret = pmalloc(sizeof(Plcontainer__Argument));
    plcontainer__argument__init(ret);

    ret->type = plcTypeToProto(&obj->type);
    ret->name = obj->name;
    ret->data = rawdataToProto(&obj->type, &obj->data);

    return ret;
}

static int protoToPlcArgument(Plcontainer__Argument *proto, plcArgument *obj) {
    protoToPlcType(proto->type, &(obj->type));
    obj->name = pstrdup(proto->name);
    protoToRawdata(proto->data, &(obj->type), &(obj->data));
    return 0;
}

plcProtoMessage *plcMsgPingToProto(plcMsgPing *msg) {
    plcProtoMessage *protoMsg = pmalloc(sizeof(plcProtoMessage));
    protoMsg->msgtype = MT_PROTOBUF;
    protoMsg->proto_type = msg->msgtype = MT_PING;

    Plcontainer__Ping *pingMsg = pmalloc(sizeof(Plcontainer__Ping));
    plcontainer__ping__init(pingMsg);
    pingMsg->version = pstrdup(PLCONTAINER_VERSION);
    
    protoMsg->body_length = plcontainer__ping__get_packed_size(pingMsg);
    protoMsg->body = pmalloc(protoMsg->body_length);
    plcontainer__ping__pack(pingMsg, protoMsg->body);

    return protoMsg; 
}

int protoToPlcMsgPing(plcProtoMessage *proto, plcMsgPing **msg) {
    *msg  = (plcMsgPing *) pmalloc(sizeof(plcMsgPing));
    (*msg)->msgtype = proto->proto_type = MT_PING;

    Plcontainer__Ping *pingMsg = plcontainer__ping__unpack(NULL, proto->body_length, proto->body);
    if (pingMsg == NULL) {
        plc_elog(ERROR, "[PROTOBUF] protoToPlcMsgPing unpack error");
    }
    return 0;
}

plcProtoMessage *plcMsgCallreqToProto(plcMsgCallreq *msg) {
    unsigned int i;
    plcProtoMessage *protoMsg = pmalloc(sizeof(plcProtoMessage));
    protoMsg->msgtype = MT_PROTOBUF;
    protoMsg->proto_type = msg->msgtype = MT_CALLREQ;
   
    Plcontainer__CallRequest *callMsg = pmalloc(sizeof(Plcontainer__CallRequest)); 
    plcontainer__call_request__init(callMsg);
    
    Plcontainer__ProcSrc *procSrc = pmalloc(sizeof(Plcontainer__ProcSrc));
    plcontainer__proc_src__init(procSrc);
    procSrc->name = pstrdup(msg->proc.name);
    procSrc->src = pstrdup(msg->proc.src);
    callMsg->proc = procSrc;
    
    callMsg->serverenc = pstrdup(msg->serverenc);
    callMsg->loglevel = msg->logLevel;
    callMsg->objectid = msg->objectid;
    callMsg->haschanged = msg->hasChanged;

    callMsg->rettype = plcTypeToProto(&msg->retType);
    callMsg->retset = msg->retset;

    callMsg->n_args = callMsg->nargs = msg->nargs;
    Plcontainer__Argument ** args = pmalloc(sizeof(Plcontainer__Argument*) * callMsg->n_args);
    for (i =0;i<callMsg->n_args;i++) {
        args[i] = plcArgumentToProto(&msg->args[i]);
    }
    callMsg->args = args;

    protoMsg->body_length = plcontainer__call_request__get_packed_size(callMsg);
    protoMsg->body = pmalloc(protoMsg->body_length);
    plcontainer__call_request__pack(callMsg, protoMsg->body);
    
    return protoMsg; 
}

int protoToPlcMsgCallreq(plcProtoMessage *proto, plcMsgCallreq **msg) {
    int i;
    *msg  = (plcMsgCallreq *) pmalloc(sizeof(plcMsgCallreq));
    (*msg)->msgtype = proto->proto_type = MT_CALLREQ;

    Plcontainer__CallRequest *callMsg = plcontainer__call_request__unpack(NULL, proto->body_length, proto->body);
    if (callMsg == NULL) {
        plc_elog(ERROR, "[PROTOBUF] protoToPlcMsgCallreq unpack error");
    }
    (*msg)->proc.name = pstrdup(callMsg->proc->name);    
    (*msg)->proc.src = pstrdup(callMsg->proc->src);
    (*msg)->serverenc = pstrdup(callMsg->serverenc);
    (*msg)->logLevel = callMsg->loglevel;
    (*msg)->objectid = callMsg->objectid;
    (*msg)->hasChanged = callMsg->haschanged;

    protoToPlcType(callMsg->rettype, &(*msg)->retType);
    (*msg)->retset = callMsg->retset;
    (*msg)->nargs = callMsg->nargs;
    if (callMsg->nargs > 0) {
        (*msg)->args = pmalloc(sizeof(plcArgument) * (*msg)->nargs);
        for (i=0;i<(*msg)->nargs;i++) {
            protoToPlcArgument(callMsg->args[i], &(*msg)->args[i]);
        }
    }

    return 0;
}

plcProtoMessage *plcMsgResultToProto(plcMsgResult *msg) {
    unsigned int i, j;
    plcProtoMessage *protoMsg = pmalloc(sizeof(plcProtoMessage));
    protoMsg->msgtype = MT_PROTOBUF;
    protoMsg->proto_type = msg->msgtype = MT_RESULT;

    Plcontainer__Result *resultMsg = pmalloc(sizeof(Plcontainer__Result));
    plcontainer__result__init(resultMsg);
    resultMsg->rows = msg->rows;
    resultMsg->cols = msg->cols;

    resultMsg->n_types = msg->cols;
    resultMsg->n_names = msg->cols;
    resultMsg->n_data = msg->rows * msg->cols;

    Plcontainer__PlcType ** types = pmalloc(sizeof(Plcontainer__PlcType*) * resultMsg->n_types);
    char **names = pmalloc(sizeof(char *) * resultMsg->n_names);
    
    for (i=0;i<msg->cols;i++) {
        types[i] = plcTypeToProto(&msg->types[i]); 
        names[i] = pstrdup(msg->names[i]); 
    }
    resultMsg->types = types;
    resultMsg->names = names;

    Plcontainer__RawData **resultData = pmalloc(sizeof(Plcontainer__RawData*) * resultMsg->n_data);
    for (i=0;i<msg->rows;i++) {
        for (j=0;j<msg->cols;j++) {
            resultData[i * msg->rows + j] = rawdataToProto(&msg->types[j], &msg->data[i][j]);
        }
    }
    resultMsg->data = resultData;

    protoMsg->body_length = plcontainer__result__get_packed_size(resultMsg);
    protoMsg->body = pmalloc(protoMsg->body_length);
    plcontainer__result__pack(resultMsg, protoMsg->body);
    
    return protoMsg; 

}

int protoToPlcMsgResult(plcProtoMessage *proto, plcMsgResult **msg) {
    uint32_t i, j;
    *msg = (plcMsgResult *) pmalloc(sizeof(plcMsgResult));
    (*msg)->msgtype = proto->proto_type = MT_RESULT;

    Plcontainer__Result *resultMsg = plcontainer__result__unpack(NULL, proto->body_length, proto->body);
    if (resultMsg == NULL) {
        plc_elog(ERROR, "[PROTOBUF] protoToPlcMsgResult unpack error");
    }

    (*msg)->rows = resultMsg->rows;
    (*msg)->cols = resultMsg->cols;

    (*msg)->types = pmalloc((*msg)->cols * sizeof(plcType));
    (*msg)->names = pmalloc((*msg)->cols * sizeof(char *));
    for (i=0;i<(*msg)->cols;i++) {
        protoToPlcType(resultMsg->types[i], &(*msg)->types[i]);
        (*msg)->names[i] = pstrdup(resultMsg->names[i]);
    }
    
    (*msg)->data = pmalloc((*msg)->rows * sizeof(rawdata *));
    for (i=0;i<(*msg)->rows;i++) {
        for (j=0;j<(*msg)->cols;j++) {
            (*msg)->data[i] = pmalloc((*msg)->cols * sizeof(rawdata));
            protoToRawdata(resultMsg->data[i * resultMsg->rows + j], &(*msg)->types[j], &(*msg)->data[i][j]);
        }
    } 

    return 0;
}
