#include "motion_stop.h"

#include <stdbool.h>
#include <stdint.h>

#include "motor_driver.h"
#include "wheel_encoder.h"

#define MOTION_STOP_SETTLE_TIME_MS 200U
#define MOTION_STOP_TIMEOUT_MS     1000U

typedef struct
{
    bool active;
    uint32_t operation_id;
    uint32_t started_at_ms;
    uint32_t last_movement_at_ms;
    WheelEncoderCounts previous_counts;
} MotionStopState;

static MotionStopState motion_stop_state;

bool motion_stop_begin(uint32_t operation_id, uint32_t now_ms)
{
    if (operation_id == 0U)
    {
        return false;
    }

    motion_stop_state = (MotionStopState){0};

    if (!motor_driver_stop())
    {
        return false;
    }

    motion_stop_state.operation_id = operation_id;
    motion_stop_state.started_at_ms = now_ms;
    motion_stop_state.last_movement_at_ms = now_ms;
    motion_stop_state.previous_counts = wheel_encoder_read();
    motion_stop_state.active = true;

    return true;
}

MotionStopCompletion motion_stop_update(uint32_t now_ms)
{
    MotionStopCompletion completion = {0};

    if (!motion_stop_state.active)
    {
        return completion;
    }

    const WheelEncoderCounts current_counts = wheel_encoder_read();
    if ((current_counts.left != motion_stop_state.previous_counts.left) ||
        (current_counts.right != motion_stop_state.previous_counts.right))
    {
        motion_stop_state.previous_counts = current_counts;
        motion_stop_state.last_movement_at_ms = now_ms;
    }

    if ((now_ms - motion_stop_state.last_movement_at_ms) >= MOTION_STOP_SETTLE_TIME_MS)
    {
        completion.completed = true;
        completion.operation_id = motion_stop_state.operation_id;
        completion.result = MOTION_STOP_RESULT_SUCCESS;
        motion_stop_state.active = false;
        return completion;
    }

    if ((now_ms - motion_stop_state.started_at_ms) >= MOTION_STOP_TIMEOUT_MS)
    {
        completion.completed = true;
        completion.operation_id = motion_stop_state.operation_id;
        completion.result = MOTION_STOP_RESULT_FAILED;
        motion_stop_state.active = false;
    }

    return completion;
}

void motion_stop_cancel(void)
{
    motion_stop_state = (MotionStopState){0};
}