/*
Copyright 1994 The PL-J Project. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE PL-J PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE PL-J PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
   OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors and should not be
interpreted as representing official policies, either expressed or implied, of the PL-J Project.
*/

/**
 * file:            commm_messages.c
 * author:            PostgreSQL developement group.
 * author:            Laszlo Hornyak
 */

/*
 * Portions Copyright © 2016-Present Pivotal Software, Inc.
 */

#include <stddef.h>
#include "common/comm_dummy.h"
#include "common/messages/messages.h"

int plc_get_type_length(plcDatatype dt) {
	int res = 0;
	switch (dt) {
		case PLC_DATA_INT1:
			res = 1;
			break;
		case PLC_DATA_INT2:
			res = 2;
			break;
		case PLC_DATA_INT4:
			res = 4;
			break;
		case PLC_DATA_INT8:
			res = 8;
			break;
		case PLC_DATA_FLOAT4:
			res = 4;
			break;
		case PLC_DATA_FLOAT8:
			res = 8;
			break;
		case PLC_DATA_TEXT:
		case PLC_DATA_UDT:
		case PLC_DATA_BYTEA:
			/* 8 = the size of pointer */
			res = 8;
			break;
		case PLC_DATA_ARRAY:
		default:
			plc_elog(ERROR, "Type %s [%d] cannot be passed plc_get_type_length function",
				        plc_get_type_name(dt), (int) dt);
			break;
	}
	return res;
}

/* Please make sure it aligns with definitions in enum plcDatatype. */
static const char *plcDatatypeName[] =
	{
		"PLC_DATA_INT1",
		"PLC_DATA_INT2",
		"PLC_DATA_INT4",
		"PLC_DATA_INT8",
		"PLC_DATA_FLOAT4",
		"PLC_DATA_FLOAT8",
		"PLC_DATA_TEXT",
		"PLC_DATA_ARRAY",
		"PLC_DATA_UDT",
		"PLC_DATA_BYTEA",
		"PLC_DATA_INVALID"
	};

const char *plc_get_type_name(plcDatatype dt) {
	return ((unsigned int) dt < PLC_DATA_MAX) ? plcDatatypeName[dt] : "UNKNOWN";
}
