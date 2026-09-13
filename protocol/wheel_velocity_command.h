#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int16_t left_velocity_mm_s;
    int16_t right_velocity_mm_s;
} WheelVelocityCommand;

#ifdef __cplusplus
}
#endif