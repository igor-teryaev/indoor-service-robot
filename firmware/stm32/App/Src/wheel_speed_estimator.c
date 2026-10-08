#include "wheel_speed_estimator.h"

#define WHEEL_CIRCUMFERENCE_MM (68.5f * 3.14159265f)
#define LEFT_COUNTS_PER_REV 1000.0f
#define RIGHT_COUNTS_PER_REV 1000.0f

void wheel_speed_estimator_init(WheelSpeedEstimator *estimator)
{
    estimator->previous_counts = (WheelEncoderCounts){0};
    estimator->previous_time_ms = 0U;
    estimator->initialized = false;
}

WheelSpeedEstimate wheel_speed_estimator_update(
    WheelSpeedEstimator *estimator,
    WheelEncoderCounts counts,
    uint32_t now_ms)
{
    WheelSpeedEstimate result = {0};

    if (!estimator->initialized)
    {
        estimator->previous_counts = counts;
        estimator->previous_time_ms = now_ms;
        estimator->initialized = true;
        return result;
    }

    const uint32_t elapsed_ms = now_ms - estimator->previous_time_ms;

    if (elapsed_ms == 0U)
    {
        return result;
    }

    const uint16_t left_delta =
        (uint16_t)(counts.left - estimator->previous_counts.left);

    const uint16_t right_delta =
        (uint16_t)(counts.right - estimator->previous_counts.right);

    result.left_mm_s =
        ((float)left_delta * WHEEL_CIRCUMFERENCE_MM * 1000.0f) /
        (LEFT_COUNTS_PER_REV * (float)elapsed_ms);

    result.right_mm_s =
        ((float)right_delta * WHEEL_CIRCUMFERENCE_MM * 1000.0f) /
        (RIGHT_COUNTS_PER_REV * (float)elapsed_ms);

    result.valid = true;

    estimator->previous_counts = counts;
    estimator->previous_time_ms = now_ms;

    return result;
}