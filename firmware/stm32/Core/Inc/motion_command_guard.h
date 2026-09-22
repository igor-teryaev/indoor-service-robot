#ifndef MOTION_COMMAND_GUARD_H
#define MOTION_COMMAND_GUARD_H

#include <stdbool.h>
#include <stdint.h>

#include "motor_driver.h"

typedef enum
{
    MOTION_COMMAND_GUARD_UPDATE_NONE = 0,
    MOTION_COMMAND_GUARD_UPDATE_STOPPED,
    MOTION_COMMAND_GUARD_UPDATE_STOP_FAILED
} MotionCommandGuardUpdate;

bool motion_command_guard_init(uint32_t timeout_ms);

bool motion_command_guard_apply(
    const MotorDriverCommand *command,
    uint32_t now_ms);

bool motion_command_guard_stop(void);

MotionCommandGuardUpdate motion_command_guard_update(
    uint32_t now_ms);

#endif