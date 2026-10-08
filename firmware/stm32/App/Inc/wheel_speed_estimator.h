#ifndef WHEEL_SPEED_ESTIMATOR_H
#define WHEEL_SPEED_ESTIMATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float left_mm_s;
    float right_mm_s;
    bool valid;
} WheelSpeedEstimate;

typedef struct
{
    WheelEncoderCounts previous_counts;
    uint32_t previous_time_ms;
    bool initialized;
} WheelSpeedEstimator;

void wheel_speed_estimator_init(WheelSpeedEstimator *estimator);

WheelSpeedEstimate wheel_speed_estimator_update(
    WheelSpeedEstimator *estimator,
    WheelEncoderCounts counts,
    uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif