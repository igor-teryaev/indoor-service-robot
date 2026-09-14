#ifndef ROBOT_PROTOCOL_WHEEL_VELOCITY_PAYLOAD_CODEC_H
#define ROBOT_PROTOCOL_WHEEL_VELOCITY_PAYLOAD_CODEC_H

#include <stdint.h>

#include "wheel_velocity_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE 8U

void wheel_velocity_payload_encode(
    const WheelVelocityPayload* payload,
    uint8_t output[WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE]);

void wheel_velocity_payload_decode(
    const uint8_t input[WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE],
    WheelVelocityPayload* payload);

#ifdef __cplusplus
}
#endif

#endif