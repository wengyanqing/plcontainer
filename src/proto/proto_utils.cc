#include "proto_utils.h"

char *plc_datum_as_udt(Datum input, plcTypeInfo *type) {
    CompositeData udt;
    PLContainerProtoUtils::DatumAsProtoData(input, type, udt);
    int size = udt.ByteSize();
    char *result = (char *)palloc(sizeof(int) + size);
    *(int *)result = size;
    udt.SerializeToArray(result+sizeof(int), size);
    plc_elog(DEBUG1, "plc_datum_as_udt call, size:%d", size);
    return result;
}

Datum plc_datum_from_udt(char *input, plcTypeInfo *type) {
    CompositeData udt;
    int size = *(int *)input;
    if (!udt.ParseFromArray(input+sizeof(int), size)) {
        plc_elog(ERROR, "plc_datum_from_udt failed");
    }
    plc_elog(DEBUG1, "plc_datum_from_udt call, size:%d", size);
    return PLContainerProtoUtils::DatumFromProtoData(udt, type);
}

char *plc_datum_as_array(Datum input, plcTypeInfo *type) {
    ArrayData arr;
    PLContainerProtoUtils::DatumAsProtoData(input, type, arr);
    int size = arr.ByteSize();
    char *result = (char *)palloc(sizeof(int) + size);
    *(int *)result = size;
    arr.SerializeToArray(result+sizeof(int), size);
    plc_elog(DEBUG1, "plc_datum_as_array call, size:%d", size);
    return result;

    return NULL;
}

Datum plc_datum_from_array(char *input, plcTypeInfo *type) {
    (void) input;
    (void) type;
    return Datum(0);
}

PlcDataType PLContainerProtoUtils::GetDataType(const plcTypeInfo *type) {
    PlcDataType ret = UNKNOWN;
    switch (type->type) {
    case PLC_DATA_INT1:
        ret = LOGICAL;
        break;
    case PLC_DATA_INT2:
    case PLC_DATA_INT4:
        ret = INT;
        break;
    case PLC_DATA_INT8:
    case PLC_DATA_FLOAT4:
    case PLC_DATA_FLOAT8:
        ret = REAL;
        break;
    case PLC_DATA_TEXT:
        ret = TEXT;
        break;
    case PLC_DATA_BYTEA:
        ret = BYTEA;
        break;
    case PLC_DATA_ARRAY:
        ret = ARRAY;
        break;
    case PLC_DATA_UDT:
        ret = COMPOSITE;
        break;
    default:
        plc_elog(ERROR, "unknown data type %d of plcType", type->type);
    }

    if (PLContainerProtoUtils::isSetOf(type)) {
        ret = SETOF;
    }

    return ret;
}

bool PLContainerProtoUtils::isSetOf(const plcTypeInfo *type) {
    int validSubTypes = 0;
    int validSubTypeIdx = -1;
    for (int i=0;i<type->nSubTypes;i++) {
        if (!type->subTypes[i].attisdropped) {
            validSubTypes += 1;
            validSubTypeIdx = i;
        }
    }

    if (type->type == PLC_DATA_ARRAY
        && validSubTypes == 1
        && type->subTypes[validSubTypeIdx].type == PLC_DATA_UDT) {
        return true;
    } else {
        return false;
    }
}

void PLContainerProtoUtils::SetScalarValue(ScalarData &data, const char *name, bool isnull, const plcTypeInfo *type, const char *value) {
    plc_elog(DEBUG1, "set scalar value, name:%s isnull:%d type:%d, value:%p", 
                name ? name : "null",
                isnull,
                type->type,
                value);

    data.set_type(PLContainerProtoUtils::GetDataType(type));
    if (name) {
        data.set_name(name);
    } else {
        data.set_name("");
    }

    data.set_isnull(isnull);
    if (value == NULL) {
        return;
    }

    switch (type->type) {
       case PLC_DATA_INT1:
            data.set_logicalvalue(*(int8_t *)value);
            break;
        case PLC_DATA_INT2:
            data.set_intvalue(*(int16_t *)value);
            break;
        case PLC_DATA_INT4:
            data.set_intvalue(*(int32_t *)value);
            break;
        case PLC_DATA_FLOAT4:
            data.set_realvalue(*(float *)value);
            break;
        case PLC_DATA_INT8:
            data.set_realvalue(*(int64_t *)value);
            break;
        case PLC_DATA_FLOAT8:
            data.set_realvalue(*(double *)value);
            break;
        case PLC_DATA_TEXT:
            data.set_stringvalue(value);
            break;
        case PLC_DATA_BYTEA:
            // int32_length + data
            data.set_byteavalue(value+sizeof(int32_t), *((int32_t *)(value)));
            break;
        default:
            plc_elog(ERROR, "invalid data type %d in sclar data", type->type);
    }
}

void PLContainerProtoUtils::DatumAsProtoData(Datum input, const plcTypeInfo *type, CompositeData &cd) {
    HeapTupleHeader rec_header;
    int i, j;

    rec_header = DatumGetHeapTupleHeader(input);
    for (i = 0, j = 0; i < type->nSubTypes; i++) {
        Datum vattr;
        bool isnull;

        if (!type->subTypes[i].attisdropped) {
            ScalarData *sd = cd.add_values();
            vattr = GetAttributeByNum(rec_header, (i + 1), &isnull);
            if (isnull) {
                PLContainerProtoUtils::SetScalarValue(*sd, type->subTypes[i].typeName, true, &type->subTypes[i], NULL);
            } else {
                PLContainerProtoUtils::SetScalarValue(*sd, type->subTypes[i].typeName, false, &type->subTypes[i], type->subTypes[i].outfunc(vattr, &type->subTypes[i]));
            }
            j++;
        }
    }
}

void PLContainerProtoUtils::DatumAsProtoData(Datum input, const plcTypeInfo *type, ArrayData &ad) {
    (void) input;
    (void) type;
    (void) ad;
  /*  
    ArrayType *array = DatumGetArrayTypeP(input);
    int ndims = ARR_NDIM(array);
    if (ndims != 1) {
        plc_elog(ERROR, "currently only support 1-dim array with scalar type element");
    }
    if (pos->bitmap && (*(pos->bitmap) & pos->bitmask) == 0) {
        res->isnull = 1;
        res->value = NULL;
    } else {
        res->isnull = 0;
        itemvalue = fetch_att(self->data, subtyp->typbyval, subtyp->typlen);
        res->value = subtyp->outfunc(itemvalue, subtyp);

        self->data = att_addlength_pointer(self->data, subtyp->typlen, self->data);
        self->data = (char *) att_align_nominal(self->data, subtyp->typalign);
    }
   */
}

Datum PLContainerProtoUtils::DatumFromProtoData(const ScalarData &sd, plcTypeInfo *type) {
    Datum retresult = (Datum)0;

    if (sd.isnull()) {
        return retresult;
    }

    char *buffer = NULL;
    switch (type->type) {
    case PLC_DATA_INT1:
        buffer = (char *)palloc(1);
        *(int8_t *)buffer = sd.logicalvalue();
        break;
    case PLC_DATA_INT2:
        buffer = (char *)palloc(2);
        *(int16_t *)buffer = sd.intvalue();
        break;
    case PLC_DATA_INT4:
        buffer = (char *)palloc(4);
        *(int32_t *)buffer = sd.intvalue();
        break;
    case PLC_DATA_INT8:
        buffer = (char *)palloc(8);
        *(int64_t *)buffer = sd.realvalue();
        break;
    case PLC_DATA_FLOAT4:
        buffer = (char *)palloc(4);
        *(float *)buffer = sd.realvalue();
        break;
    case PLC_DATA_FLOAT8:
        buffer = (char *)palloc(8);
        *(double *)buffer = sd.realvalue();
        break;
    case PLC_DATA_TEXT:
        buffer = (char *)palloc(sd.stringvalue().size()+1);
        strncpy(buffer, sd.stringvalue().c_str(), sd.stringvalue().size()+1);
        break;
    case PLC_DATA_BYTEA:
        buffer = (char *)palloc(sd.stringvalue().size()+sizeof(int32_t));
        *(int32_t *)buffer = sd.stringvalue().size();
        memcpy(buffer+sizeof(int32_t), sd.stringvalue().data(), sd.stringvalue().size());
        break;
    default:
        plc_elog(ERROR, "unknown scalar type:%d", type->type);
    }

    if (buffer) {
        retresult = type->infunc(buffer, type);
        pfree(buffer);
    }

    return retresult;
}

Datum PLContainerProtoUtils::DatumFromProtoData(const CompositeData &cd, plcTypeInfo *type) {
    TupleDesc desc;
    HeapTuple tuple;
    Datum *values;
    bool *nulls;
    volatile int i, j;
    
    /* Build tuple */
    values = (Datum *)palloc(sizeof(Datum) * type->nSubTypes);
    nulls = (bool *)palloc(sizeof(bool) * type->nSubTypes);
    for (i = 0, j = 0; i < type->nSubTypes; ++i) {
        if (!type->subTypes[i].attisdropped) {
            if (cd.values(i).isnull()) {
                nulls[i] = true;
                values[i] = (Datum) 0;
            } else {
                nulls[i] = false;
                values[i] = PLContainerProtoUtils::DatumFromProtoData(cd.values(i), type);
            }
            j++;
        }
    }

    desc = lookup_rowtype_tupdesc(type->typeOid, type->typmod);
    tuple = heap_form_tuple(desc, values, nulls);
    ReleaseTupleDesc(desc);

    pfree(values);
    pfree(nulls);

    return HeapTupleGetDatum(tuple);
}
