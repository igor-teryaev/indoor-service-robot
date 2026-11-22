#ifndef ROBOT_PROTOCOL_WHEEL_SPEED_TELEMETRY_PAYLOAD_H
#define ROBOT_PROTOCOL_WHEEL_SPEED_TELEMETRY_PAYLOAD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t left_speed_mm_s;
    uint16_t right_speed_mm_s;
    bool valid;
} WheelSpeedTelemetryPayload;

#ifdef __cplusplus
}
#endif

#endif