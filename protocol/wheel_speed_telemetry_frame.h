#ifndef ROBOT_PROTOCOL_WHEEL_SPEED_TELEMETRY_FRAME_H
#define ROBOT_PROTOCOL_WHEEL_SPEED_TELEMETRY_FRAME_H

#include <stdint.h>

#include "protocol_frame.h"
#include "wheel_speed_telemetry_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Builds an unsolicited wheel speed telemetry frame.
 * The caller owns the telemetry sequence counter.
 * The payload pointer must not be NULL.
 */
ProtocolFrame wheel_speed_telemetry_frame_build(
    const WheelSpeedTelemetryPayload *payload,
    uint16_t sequence);

#ifdef __cplusplus
}
#endif

#endif