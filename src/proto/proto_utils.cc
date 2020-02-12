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
    return PLContainerProtoUtils::DatumFromProtoData(*(CompositeData *)input, type);
}

char *plc_datum_as_array(Datum input, plcTypeInfo *type) {
    int size = 0;
    char *result;
    if (type->subTypes && type->subTypes[0].type == PLC_DATA_UDT) {
        SetOfData setof;
        PLContainerProtoUtils::DatumAsProtoData(input, type, setof);
        size = setof.ByteSize();
        result = (char *)palloc(sizeof(int) + size);
        *(int *)result = size;
        setof.SerializeToArray(result+sizeof(int), size);
        plc_elog(DEBUG1, "plc_datum_as_array call, setof size:%d", size);

    } else {
        ArrayData arr;
        PLContainerProtoUtils::DatumAsProtoData(input, type, arr);
        size = arr.ByteSize();
        result = (char *)palloc(sizeof(int) + size);
        *(int *)result = size;
        arr.SerializeToArray(result+sizeof(int), size);
        plc_elog(DEBUG1, "plc_datum_as_array call, size:%d", size);
    }

    return result;
}

Datum plc_datum_from_array(char *input, plcTypeInfo *type) {
    if (type && type->nSubTypes > 0 && type->subTypes[0].type == PLC_DATA_UDT) {
        // array of UDT
        return PLContainerProtoUtils::DatumFromProtoData(*(SetOfData *)input, type);
    } else {
        // array of Scalar
        return PLContainerProtoUtils::DatumFromProtoData(*(ArrayData *)input, type);
    }
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
    case PLC_DATA_VOID:
        ret = VOID;
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
    PLContainerProtoUtils::DatumAsProtoArrayOrSetOf(input, type, &ad, NULL);
}

void PLContainerProtoUtils::DatumAsProtoData(Datum input, const plcTypeInfo *type, SetOfData &setof) {
    PLContainerProtoUtils::DatumAsProtoArrayOrSetOf(input, type, NULL, &setof);
}

void PLContainerProtoUtils::DatumAsProtoArrayOrSetOf(Datum input, const plcTypeInfo *type, ArrayData *ad, SetOfData *setof) {
    bool isSetOf = false;
    if (!ad && setof) {
        isSetOf = true;
    }

    ArrayType *array = DatumGetArrayTypeP(input);
    int ndims = ARR_NDIM(array);
    if (ndims != 1) {
        plc_elog(ERROR, "currently only support 1-dim array or setof");
    }

    bits8   *bitmap = ARR_NULLBITMAP(array);
    int     bitmask = 1;
    int nitems = ArrayGetNItems(ARR_NDIM(array), ARR_DIMS(array));
    char *data = ARR_DATA_PTR(array);
    plcTypeInfo *elementType = &type->subTypes[0];

    Datum itemvalue;
    int curitem = 0;

    while (true) {
        if (curitem >= nitems) {
            break;
        }
        curitem++;

        if (elementType->type != PLC_DATA_UDT) {
            ScalarData *sd = ad->add_values();
     
            if (bitmap && (*bitmap & bitmask) == 0) {
                PLContainerProtoUtils::SetScalarValue(*sd, elementType->typeName, true, elementType, NULL);
            } else {
                itemvalue = fetch_att(data, elementType->typbyval, elementType->typlen);
                PLContainerProtoUtils::SetScalarValue(*sd, elementType->typeName, false, elementType, elementType->outfunc(itemvalue, elementType));
                data = att_addlength_pointer(data, elementType->typlen, data);
                data = (char *) att_align_nominal(data, elementType->typalign);
            }
        } else {
            PLContainerProtoUtils::DatumAsProtoData(PointerGetDatum(data), elementType, *setof->add_rowvalues());
            data = att_addlength_pointer(data, elementType->typlen, data);
            data = (char *) att_align_nominal(data, elementType->typalign);
        }

        if (bitmap) {
            bitmask <<= 1;
            if (bitmask == 0x100 ) {
                bitmap++;
                bitmask = 1;
            }
        }
    }

    if (!isSetOf) {
        ad->set_elementtype(PLContainerProtoUtils::GetDataType(elementType));
    } else if (setof->rowvalues_size() > 0) {
        for (int i=0;i<setof->rowvalues(0).values_size();i++) {
            setof->add_columnnames(setof->rowvalues(0).values(i).name());
            setof->add_columntypes(setof->rowvalues(0).values(i).type());
        }
    }
}

Datum PLContainerProtoUtils::DatumFromProtoData(const ScalarData &sd, plcTypeInfo *type, bool isArrayElement) {
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
        if (!isArrayElement) {
            retresult = type->infunc(buffer, type);
        } else {
            retresult = type->infunc((char *)&buffer, type);
        }
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
                values[i] = PLContainerProtoUtils::DatumFromProtoData(cd.values(i), &type->subTypes[i]);
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

Datum PLContainerProtoUtils::DatumFromProtoData(const ArrayData &ad, plcTypeInfo *type) {
    Datum retresult = (Datum)0;
    int         dims[1];
    int         lbs[1];

    plcTypeInfo *subType = &type->subTypes[0];
    int nelems = ad.values_size();
    dims[0] = nelems;
    lbs[0] = 1;

    Datum *elems = (Datum *)palloc(nelems * sizeof(Datum));
    bool *nulls = (bool *)palloc(nelems * sizeof(bool));
    for (int i=0;i<nelems;i++) {
        nulls[i] = ad.values(i).isnull();
        if (ad.values(i).isnull()) {
            elems[i] = Datum(0);
        } else if (subType->type == PLC_DATA_TEXT || subType->type == PLC_DATA_BYTEA) {
            elems[i] = PLContainerProtoUtils::DatumFromProtoData(ad.values(i), subType, true);
        } else {
            elems[i] = PLContainerProtoUtils::DatumFromProtoData(ad.values(i), subType, false);
        }
    }

    ArrayType *array = construct_md_array(elems,
                                        nulls,
                                        1,
                                        dims,
                                        lbs,
                                        subType->typeOid,
                                        subType->typlen,
                                        subType->typbyval,
                                        subType->typalign);

    retresult = PointerGetDatum(array);

    pfree(elems);
    pfree(nulls);

    return retresult;
}

Datum PLContainerProtoUtils::DatumFromProtoData(const SetOfData &ad, plcTypeInfo *type) {
    Datum retresult = (Datum)0;
    int         dims[1];
    int         lbs[1];

    plcTypeInfo *subType = &type->subTypes[0];
    int nelems = ad.rowvalues_size();
    dims[0] = nelems;
    lbs[0] = 1;

    Datum *elems = (Datum *)palloc(nelems * sizeof(Datum));
    bool *nulls = (bool *)palloc(nelems * sizeof(bool));
    for (int i=0;i<nelems;i++) {
        elems[i] = PLContainerProtoUtils::DatumFromProtoData(ad.rowvalues(i), subType);
        nulls[i] = false; 
    }

    ArrayType *array = construct_md_array(elems,
                                        nulls,
                                        1,
                                        dims,
                                        lbs,
                                        subType->typeOid,
                                        subType->typlen,
                                        subType->typbyval,
                                        subType->typalign);

    retresult = PointerGetDatum(array);

    pfree(elems);
    pfree(nulls);

    return retresult;
}
