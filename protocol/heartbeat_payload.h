#ifndef ROBOT_PROTOCOL_HEARTBEAT_PAYLOAD_H
#define ROBOT_PROTOCOL_HEARTBEAT_PAYLOAD_H

#include <stdint.h>

#include "link_state.h"

typedef struct
{
    LinkState link_state;
    uint32_t uptime_ms;
} HeartbeatPayload;

#endif