#include "wheel_speed_telemetry_adapter.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

static uint16_t convert_speed(float speed_mm_s)
{
    if (speed_mm_s >= (float)UINT16_MAX)
    {
        return UINT16_MAX;
    }

    return (uint16_t)(speed_mm_s + 0.5f);
}

WheelSpeedTelemetryPayload
wheel_speed_telemetry_payload_from_estimate(
    const WheelSpeedEstimate *estimate)
{
    WheelSpeedTelemetryPayload payload = {0};

    if ((estimate == NULL) ||
        !estimate->valid ||
        !isfinite(estimate->left_mm_s) ||
        !isfinite(estimate->right_mm_s) ||
        (estimate->left_mm_s < 0.0f) ||
        (estimate->right_mm_s < 0.0f))
    {
        return payload;
    }

    payload.left_speed_mm_s =
        convert_speed(estimate->left_mm_s);

    payload.right_speed_mm_s =
        convert_speed(estimate->right_mm_s);

    payload.valid = true;

    return payload;
}