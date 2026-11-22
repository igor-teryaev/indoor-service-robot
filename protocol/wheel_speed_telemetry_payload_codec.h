#ifndef ROBOT_PROTOCOL_WHEEL_SPEED_TELEMETRY_PAYLOAD_CODEC_H
#define ROBOT_PROTOCOL_WHEEL_SPEED_TELEMETRY_PAYLOAD_CODEC_H

#include <stdint.h>

#include "wheel_speed_telemetry_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE 5U

void wheel_speed_telemetry_payload_encode(
    const WheelSpeedTelemetryPayload* payload,
    uint8_t output[WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE]);

void wheel_speed_telemetry_payload_decode(
    const uint8_t input[WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE],
    WheelSpeedTelemetryPayload* payload);

#ifdef __cplusplus
}
#endif

#endif