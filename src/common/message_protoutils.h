/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_PROTOUTILS_H
#define PLC_MESSAGE_PROTOUTILS_H 

#include "messages/messages.h"

// MT_PING
plcProtoMessage     *plcMsgPingToProto(plcMsgPing *msg);
int                 protoToPlcMsgPing(plcProtoMessage *proto, plcMsgPing **msg);

// MT_CALLREQ
plcProtoMessage     *plcMsgCallreqToProto(plcMsgCallreq *msg);
int                 protoToPlcMsgCallreq(plcProtoMessage *proto, plcMsgCallreq **msg);

// MT_RESULT
plcProtoMessage     *plcMsgResultToProto(plcMsgResult *msg);
int                 protoToPlcMsgResult(plcProtoMessage *proto, plcMsgResult **msg);

#endif /* PLC_MESSAGE_PROTOUTILS_H*/
