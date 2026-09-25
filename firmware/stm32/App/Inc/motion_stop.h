#ifndef MOTION_STOP_H
#define MOTION_STOP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MOTION_STOP_RESULT_NONE = 0,
    MOTION_STOP_RESULT_SUCCESS,
    MOTION_STOP_RESULT_FAILED
} MotionStopResult;

typedef struct
{
    bool completed;
    uint32_t operation_id;
    MotionStopResult result;
} MotionStopCompletion;

bool motion_stop_begin(uint32_t operation_id, uint32_t now_ms);
MotionStopCompletion motion_stop_update(uint32_t now_ms);
void motion_stop_cancel(void);

#endif