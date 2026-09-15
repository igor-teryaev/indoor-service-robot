#ifndef ROBOT_PROTOCOL_MOTION_ACK_CODEC_H
#define ROBOT_PROTOCOL_MOTION_ACK_CODEC_H

#include <stdint.h>

#include "motion_ack_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_ACK_WIRE_SIZE 1U

void motion_ack_encode(
    const MotionAckPayload* payload,
    uint8_t output[MOTION_ACK_WIRE_SIZE]);

void motion_ack_decode(
    const uint8_t input[MOTION_ACK_WIRE_SIZE],
    MotionAckPayload* payload);

#ifdef __cplusplus
}
#endif

#endif