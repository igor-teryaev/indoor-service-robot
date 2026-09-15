#ifndef ROBOT_PROTOCOL_LINK_SYNC_CODEC_H
#define ROBOT_PROTOCOL_LINK_SYNC_CODEC_H

#include <stdint.h>

#include "link_sync_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_SYNC_WIRE_SIZE 8U

void link_sync_encode(
    const LinkSyncPayload* payload,
    uint8_t output[LINK_SYNC_WIRE_SIZE]);

void link_sync_decode(
    const uint8_t input[LINK_SYNC_WIRE_SIZE],
    LinkSyncPayload* payload);

#ifdef __cplusplus
}
#endif

#endif