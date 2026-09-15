#ifndef ROBOT_PROTOCOL_MOTION_RESPONSE_CODEC_H
#define ROBOT_PROTOCOL_MOTION_RESPONSE_CODEC_H

#include <stdint.h>

#include "motion_response_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_RESPONSE_WIRE_SIZE 6U

void motion_response_encode(
    const MotionResponsePayload* payload,
    uint8_t output[MOTION_RESPONSE_WIRE_SIZE]);

void motion_response_decode(
    const uint8_t input[MOTION_RESPONSE_WIRE_SIZE],
    MotionResponsePayload* payload);

#ifdef __cplusplus
}
#endif

#endif