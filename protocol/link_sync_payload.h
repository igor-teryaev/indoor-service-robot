#ifndef ROBOT_PROTOCOL_LINK_SYNC_PAYLOAD_H
#define ROBOT_PROTOCOL_LINK_SYNC_PAYLOAD_H

#include <stdint.h>

typedef struct
{
    uint64_t sync_token;
} LinkSyncPayload;

#endif