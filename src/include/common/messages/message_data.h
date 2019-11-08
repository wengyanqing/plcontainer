/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_DATA_H
#define PLC_MESSAGE_DATA_H

#include "message_base.h"

typedef struct plcIterator plcIterator;

typedef struct plcArrayMeta {
	plcDatatype type;  // deprecated - should be moved to payload if required
	int32_t ndims;
	int32_t *dims;
	int32_t size;  // deprecated - should be moved to payload if required
} plcArrayMeta;

typedef struct plcArray {
	plcArrayMeta *meta;
	char *data;
	char *nulls;
} plcArray;

struct plcIterator {
	plcArrayMeta *meta;
	char *data;
	char *position;
	char *payload;

	/*
	 * used to return next element from client-side array structure to avoid
	 * creating a copy of full array before sending it
	 */
	rawdata *(*next)(plcIterator *self);

	/*
	 * called after data is sent to free data
	 */
	void (*cleanup)(plcIterator *self);
};

plcArray *plc_alloc_array(int ndims);

void plc_free_array(plcArray *arr, plcType *type, bool isSender);

#endif /* PLC_MESSAGE_DATA_H */
