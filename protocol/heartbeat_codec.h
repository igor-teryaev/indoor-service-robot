#ifndef ROBOT_PROTOCOL_HEARTBEAT_CODEC_H
#define ROBOT_PROTOCOL_HEARTBEAT_CODEC_H

#include <stdint.h>

#include "heartbeat_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEARTBEAT_WIRE_SIZE 5U

void heartbeat_encode(
    const HeartbeatPayload* payload,
    uint8_t output[HEARTBEAT_WIRE_SIZE]);

void heartbeat_decode(
    const uint8_t input[HEARTBEAT_WIRE_SIZE],
    HeartbeatPayload* payload);

#ifdef __cplusplus
}
#endif

#endif