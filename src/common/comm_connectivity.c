/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/comm_connectivity.h"
#include "common/comm_dummy.h"

void plcContextInit(plcContext *ctx)
{
	ctx->service_address = NULL;
}

/*
 * This function only release buffers and reset plan array.
 * NOTE: socket fd keeps open and service address is still valid.
 * We INTEND to reuse connection to the container.
 */
void plcReleaseContext(plcContext *ctx)
{
	(void) ctx;
}

/*
 *  This function is used for re-init buffer and plan slot
 *  when a new query coming and connection is resued.
 */

void plcContextReset(plcContext *ctx)
{
	(void) ctx;
}

/*
 * clearup the container connection context
 */
void plcFreeContext(plcContext *ctx)
{
	plcReleaseContext(ctx);
	pfree(ctx->service_address);
	pfree(ctx);
}
