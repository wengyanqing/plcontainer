#ifndef __PROTO_UTILS_H__
#define __PROTO_UTILS_H__

#include "client.h"

using namespace plcontainer;

class PLContainerProtoUtils {
public:
    static void DatumAsProtoData(Datum input, const plcTypeInfo *type, CompositeData &cd);
    static void DatumAsProtoData(Datum input, const plcTypeInfo *type, ArrayData &cd);
    static void DatumAsProtoData(Datum input, const plcTypeInfo *type, SetOfData &setof);

    static Datum DatumFromProtoData(const ScalarData    &sd, plcTypeInfo *type, bool isArrayElement = false);
    static Datum DatumFromProtoData(const CompositeData &cd, plcTypeInfo *type);
    static Datum DatumFromProtoData(const ArrayData &ad, plcTypeInfo *type);
    static Datum DatumFromProtoData(const SetOfData &ad, plcTypeInfo *type);
 
    static void SetScalarValue(ScalarData &data, const char *name, bool isnull, const plcTypeInfo *type, const char *value);
    static PlcDataType GetDataType(const plcTypeInfo *type);
private:
    static bool isSetOf(const plcTypeInfo *type);
    static void DatumAsProtoArrayOrSetOf(Datum input, const plcTypeInfo *type, ArrayData *ad, SetOfData *setof);
};

#endif 
