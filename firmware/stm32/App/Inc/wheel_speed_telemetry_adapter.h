#ifndef ROBOT_WHEEL_SPEED_TELEMETRY_ADAPTER_H
#define ROBOT_WHEEL_SPEED_TELEMETRY_ADAPTER_H

#include "wheel_speed_estimator.h"
#include "wheel_speed_telemetry_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

WheelSpeedTelemetryPayload
wheel_speed_telemetry_payload_from_estimate(
    const WheelSpeedEstimate *estimate);

#ifdef __cplusplus
}
#endif

#endif