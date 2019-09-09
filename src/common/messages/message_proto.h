/*------------------------------------------------------------------------------
 *
 *
 * Copyright (c) 2016, Pivotal.
 *
 *------------------------------------------------------------------------------
 */
#ifndef PLC_MESSAGE_PROTO_H
#define PLC_MESSAGE_PROTO_H

#include "../proto/plcontainer.pb-c.h"

typedef struct plcProtoMessage {
    base_message_content
    uint32_t    proto_type;
    uint32_t    body_length;
    uint8_t     *body;
} plcProtoMessage;

#endif /* PLC_MESSAGE_PROTO_H */
