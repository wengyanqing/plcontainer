/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */


/* Greenplum headers */
#include "postgres.h"
#include "fmgr.h"
#include "access/transam.h"
#include "access/tupmacs.h"

#ifdef PLC_PG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "executor/spi.h"
#ifdef PLC_PG
#pragma GCC diagnostic pop
#endif

#include "parser/parse_type.h"
#include "utils/fmgroids.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"
#include "utils/syscache.h"
#include "utils/builtins.h"

#ifdef PLC_PG
  #include "catalog/pg_type.h"
  #include "access/htup_details.h"
#endif

#include "common/comm_dummy.h"
#include "plc/plc_typeio.h"
#include "plc/message_fns.h"

#include "interface.h"

static void fill_type_info_inner(FunctionCallInfo fcinfo, Oid typeOid, plcTypeInfo *type,
                                 bool isArrayElement, bool isUDTElement);

static char *plc_datum_as_int1(Datum input, plcTypeInfo *type);

static char *plc_datum_as_int2(Datum input, plcTypeInfo *type);

static char *plc_datum_as_int4(Datum input, plcTypeInfo *type);

static char *plc_datum_as_int8(Datum input, plcTypeInfo *type);

static char *plc_datum_as_float4(Datum input, plcTypeInfo *type);

static char *plc_datum_as_float8(Datum input, plcTypeInfo *type);

static char *plc_datum_as_float8_numeric(Datum input, plcTypeInfo *type);

static char *plc_datum_as_text(Datum input, plcTypeInfo *type);

static char *plc_datum_as_bytea(Datum input, plcTypeInfo *type);

static char *plc_datum_as_void(Datum input, plcTypeInfo *type);

static Datum plc_datum_from_int1(char *input, plcTypeInfo *type);

static Datum plc_datum_from_int2(char *input, plcTypeInfo *type);

static Datum plc_datum_from_int4(char *input, plcTypeInfo *type);

static Datum plc_datum_from_int8(char *input, plcTypeInfo *type);

static Datum plc_datum_from_float4(char *input, plcTypeInfo *type);

static Datum plc_datum_from_float8(char *input, plcTypeInfo *type);

static Datum plc_datum_from_float8_numeric(char *input, plcTypeInfo *type);

static Datum plc_datum_from_text(char *input, plcTypeInfo *type);

static Datum plc_datum_from_text_ptr(char *input, plcTypeInfo *type);

static Datum plc_datum_from_bytea(char *input, plcTypeInfo *type);

static Datum plc_datum_from_bytea_ptr(char *input, plcTypeInfo *type);

static Datum plc_datum_from_udt_ptr(char *input, plcTypeInfo *type);

static Datum plc_datum_from_void(char *input, plcTypeInfo *type);

static void
fill_type_info_inner(FunctionCallInfo fcinfo, Oid typeOid, plcTypeInfo *type, bool isArrayElement, bool isUDTElement) {
	HeapTuple typeTup;
	Form_pg_type typeStruct;
	char dummy_delim;
	Oid typioparam;

	/* Since this is recursive, it could theoretically be driven to overflow */
	check_stack_depth();

	if (get_typtype(typeOid) == TYPTYPE_DOMAIN) {
		plc_elog(ERROR, "plcontainer does not support domain type");
	}
	typeTup = SearchSysCache(TYPEOID, ObjectIdGetDatum(typeOid), 0, 0, 0);
	if (!HeapTupleIsValid(typeTup))
		plc_elog(ERROR, "cache lookup failed for type %u", typeOid);

	typeStruct = (Form_pg_type) GETSTRUCT(typeTup);
	ReleaseSysCache(typeTup);

	type->typeOid = typeOid;
	type->output = typeStruct->typoutput;
	type->input = typeStruct->typinput;
	get_type_io_data(typeOid, IOFunc_input,
	                 &type->typlen, &type->typbyval, &type->typalign,
	                 &dummy_delim,
	                 &typioparam, &type->input);
	type->typmod = typeStruct->typtypmod;
	type->nSubTypes = 0;
	type->subTypes = NULL;
	type->typelem = typeStruct->typelem;

	type->is_rowtype = false;
	type->is_record = false;
	type->attisdropped = false;
	type->typ_relid = InvalidOid;
	type->typrel_xmin = InvalidTransactionId;
	ItemPointerSetInvalid(&type->typrel_tid);
	type->typeName = NULL;

	switch (typeOid) {
		case BOOLOID:
			type->type = PLC_DATA_INT1;
			type->outfunc = plc_datum_as_int1;
			type->infunc = plc_datum_from_int1;
			break;
		case INT2OID:
			type->type = PLC_DATA_INT2;
			type->outfunc = plc_datum_as_int2;
			type->infunc = plc_datum_from_int2;
			break;
		case INT4OID:
			type->type = PLC_DATA_INT4;
			type->outfunc = plc_datum_as_int4;
			type->infunc = plc_datum_from_int4;
			break;
		case INT8OID:
			type->type = PLC_DATA_INT8;
			type->outfunc = plc_datum_as_int8;
			type->infunc = plc_datum_from_int8;
			break;
		case FLOAT4OID:
			type->type = PLC_DATA_FLOAT4;
			type->outfunc = plc_datum_as_float4;
			type->infunc = plc_datum_from_float4;
			break;
		case FLOAT8OID:
			type->type = PLC_DATA_FLOAT8;
			type->outfunc = plc_datum_as_float8;
			type->infunc = plc_datum_from_float8;
			break;
		case NUMERICOID:
			type->type = PLC_DATA_FLOAT8;
			type->outfunc = plc_datum_as_float8_numeric;
			type->infunc = plc_datum_from_float8_numeric;
			break;
		case BYTEAOID:
			type->type = PLC_DATA_BYTEA;
			type->outfunc = plc_datum_as_bytea;
			if (!isArrayElement) {
				type->infunc = plc_datum_from_bytea;
			} else {
				type->infunc = plc_datum_from_bytea_ptr;
			}
			break;
        case VOIDOID:
            type->type = PLC_DATA_VOID;
            type->outfunc = plc_datum_as_void;
            type->infunc = plc_datum_from_void;
            break;
			/* All the other types are passed through in-out functions to translate
			 * them to text before sending and after receiving */
		default:
			type->type = PLC_DATA_TEXT;
			type->outfunc = plc_datum_as_text;
			if (!isArrayElement) {
				type->infunc = plc_datum_from_text;
			} else {
				type->infunc = plc_datum_from_text_ptr;
			}
			break;
	}

	/* Processing arrays here */
	if (!isArrayElement && typeStruct->typelem != 0 && typeStruct->typoutput == F_ARRAY_OUT) {
		type->type = PLC_DATA_ARRAY;
		type->outfunc = plc_datum_as_array;
		type->infunc = plc_datum_from_array;
		type->nSubTypes = 1;
		type->subTypes = (plcTypeInfo *) top_palloc(sizeof(plcTypeInfo));
		fill_type_info_inner(fcinfo, typeStruct->typelem, &type->subTypes[0], true, isUDTElement);
	}

	/* Processing composite types - only first level is supported */
	if (!isUDTElement) {
		TupleDesc desc;

		if (typeOid == RECORDOID) {
			if (fcinfo == NULL || get_call_result_type(fcinfo, NULL, &desc) != TYPEFUNC_COMPOSITE) {
				ereport(ERROR,
				        (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					        errmsg("function returning record called in context "
						               "that cannot accept type record")));
			}
			type->is_rowtype = true;

			/* bless the record to make it known to the typcache lookup code */
			BlessTupleDesc(desc);

			/* save the freshly generated typmod */
			type->typmod = desc->tdtypmod;

			/* Indicate that this type is a record */
			type->is_record = true;
		}

		if (typeStruct->typtype == TYPTYPE_COMPOSITE) {
			desc = lookup_rowtype_tupdesc(type->typeOid, type->typmod);
			type->is_rowtype = true;
		}

		if (type->is_rowtype) {
			int i;

			type->type = PLC_DATA_UDT;
			type->outfunc = plc_datum_as_udt;
			if (!isArrayElement) {
				type->infunc = plc_datum_from_udt;
			} else {
				type->infunc = plc_datum_from_udt_ptr;
			}
			type->nSubTypes = desc->natts;

			if (desc->tdtypeid != RECORDOID && !TransactionIdIsValid(type->typrel_xmin)) {
				HeapTuple relTup;

				/* Get the pg_class tuple corresponding to the type of the input */
				type->typ_relid = typeidTypeRelid(desc->tdtypeid);
				relTup = SearchSysCache1(RELOID, ObjectIdGetDatum(type->typ_relid));
				if (!HeapTupleIsValid(relTup)) {
					plc_elog(ERROR, "cache lookup failed for relation %u", type->typ_relid);
				}

				/* Extract the XMIN value to later use it in PLy_procedure_valid */
				type->typrel_xmin = HeapTupleHeaderGetXmin(relTup->t_data);
				type->typrel_tid = relTup->t_self;
				type->typeName = plc_top_strdup(NameStr(typeStruct->typname));

				ReleaseSysCache(relTup);
			}

			// Allocate memory for this number of arguments
			type->subTypes = (plcTypeInfo *) top_palloc(type->nSubTypes * sizeof(plcTypeInfo));
			memset(type->subTypes, 0, type->nSubTypes * sizeof(plcTypeInfo));

			// Fill all the subtypes
			for (i = 0; i < desc->natts; i++) {
				type->subTypes[i].attisdropped = desc->attrs[i]->attisdropped;
				if (!type->subTypes[i].attisdropped) {
					/* We support the case with array of UDTs, each of which contains another array */
					fill_type_info_inner(fcinfo, desc->attrs[i]->atttypid, &type->subTypes[i], false, true);
				}
				type->subTypes[i].typeName = plc_top_strdup(NameStr(desc->attrs[i]->attname));
			}

			ReleaseTupleDesc(desc);
		}
	}
}

void fill_type_info(FunctionCallInfo fcinfo, Oid typeOid, plcTypeInfo *type) {
	fill_type_info_inner(fcinfo, typeOid, type, false, false);
}

void free_type_info(plcTypeInfo *type) {
	int i = 0;

	if (type->typeName != NULL) {
		pfree(type->typeName);
	}

	for (i = 0; i < type->nSubTypes; i++) {
		free_type_info(&type->subTypes[i]);
	}

	if (type->nSubTypes > 0) {
		pfree(type->subTypes);
	}
}

static char *plc_datum_as_int1(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) palloc(1);
	*((char *) out) = DatumGetBool(input);
	return out;
}

static char *plc_datum_as_int2(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) palloc(2);
	*((int16 *) out) = DatumGetInt16(input);
	return out;
}

static char *plc_datum_as_int4(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) palloc(4);
	*((int32 *) out) = DatumGetInt32(input);
	return out;
}

static char *plc_datum_as_int8(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) palloc(8);
	*((int64 *) out) = DatumGetInt64(input);
	return out;
}

static char *plc_datum_as_float4(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) palloc(4);
	*((float4 *) out) = DatumGetFloat4(input);
	return out;
}

static char *plc_datum_as_float8(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) palloc(8);
	*((float8 *) out) = DatumGetFloat8(input);
	return out;
}

static char *plc_datum_as_float8_numeric(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	char *out = (char *) palloc(8);
	/* Numeric is casted to float8 which causes precision lost */
	Datum fdatum = DirectFunctionCall1(numeric_float8, input);
	*((float8 *) out) = DatumGetFloat8(fdatum);
	return out;
}

static char *plc_datum_as_text(Datum input, plcTypeInfo *type) {
	return DatumGetCString(OidFunctionCall3(type->output,
	                                        input,
	                                        type->typelem,
	                                        type->typmod));
}

static char *plc_datum_as_bytea(Datum input, pg_attribute_unused() plcTypeInfo *type) {
	text *txt = DatumGetByteaP(input);
	int len = VARSIZE(txt) - VARHDRSZ;
	char *out = (char *) palloc(len + 4);
	*((int *) out) = len;
	memcpy(out + 4, VARDATA(txt), len);
	return out;
}

static char *plc_datum_as_void(pg_attribute_unused() Datum input, pg_attribute_unused() plcTypeInfo *type) {
    return (char *)pstrdup(""); 
}

static Datum plc_datum_from_int1(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return BoolGetDatum(*((bool *) input));
}

static Datum plc_datum_from_int2(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Int16GetDatum(*((int16 *) input));
}

static Datum plc_datum_from_int4(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Int32GetDatum(*((int32 *) input));
}

static Datum plc_datum_from_int8(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Int64GetDatum(*((int64 *) input));
}

static Datum plc_datum_from_float4(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Float4GetDatum(*((float4 *) input));
}

static Datum plc_datum_from_float8(char *input, pg_attribute_unused() plcTypeInfo *type) {
	return Float8GetDatum(*((float8 *) input));
}

static Datum plc_datum_from_float8_numeric(char *input, pg_attribute_unused() plcTypeInfo *type) {
	Datum fdatum = Float8GetDatum(*((float8 *) input));
	return DirectFunctionCall1(float8_numeric, fdatum);
}

static Datum plc_datum_from_text(char *input, plcTypeInfo *type) {
	return OidFunctionCall3(type->input,
	                        CStringGetDatum(input),
	                        type->typelem,
	                        type->typmod);
}

static Datum plc_datum_from_text_ptr(char *input, plcTypeInfo *type) {
	return OidFunctionCall3(type->input,
	                        CStringGetDatum(*((char **) input)),
	                        type->typelem,
	                        type->typmod);
}

static Datum plc_datum_from_bytea(char *input, pg_attribute_unused() plcTypeInfo *type) {
	int size = *((int *) input);
	bytea *result = palloc(size + VARHDRSZ);

	SET_VARSIZE(result, size + VARHDRSZ);
	memcpy(VARDATA(result), input + 4, size);
	return PointerGetDatum(result);
}

static Datum plc_datum_from_bytea_ptr(char *input, plcTypeInfo *type) {
	return plc_datum_from_bytea(*((char **) input), type);
}

static Datum plc_datum_from_udt_ptr(char *input, plcTypeInfo *type) {
	return plc_datum_from_udt(*((char **) input), type);
}

static Datum plc_datum_from_void(pg_attribute_unused() char *input, pg_attribute_unused() plcTypeInfo *type) {
    return (Datum)0;
}

plcDatatype plc_get_datatype_from_oid(Oid oid) {
	plcDatatype dt;

	switch (oid) {
		case BOOLOID:
			dt = PLC_DATA_INT1;
			break;
		case INT2OID:
			dt = PLC_DATA_INT2;
			break;
		case INT4OID:
			dt = PLC_DATA_INT4;
			break;
		case INT8OID:
			dt = PLC_DATA_INT8;
			break;
		case FLOAT4OID:
			dt = PLC_DATA_FLOAT4;
			break;
		case FLOAT8OID:
			dt = PLC_DATA_FLOAT8;
			break;
		case NUMERICOID:
			dt = PLC_DATA_FLOAT8;
			break;
		case BYTEAOID:
			dt = PLC_DATA_BYTEA;
			break;
		default:
			dt = PLC_DATA_TEXT;
			break;
	}

	return dt;
}
