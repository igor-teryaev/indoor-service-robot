#include "motion_command_guard.h"

#include <stddef.h>

#include "motor_driver.h"
#include "motion_watchdog.h"

typedef struct
{
    bool initialized;
    bool stop_pending;
} MotionCommandGuardState;

static MotionCommandGuardState motion_command_guard_state;

static bool motion_command_guard_try_stop(void)
{
    if (motor_driver_stop())
    {
        motion_watchdog_disarm();
        motion_command_guard_state.stop_pending = false;
        return true;
    }

    motion_command_guard_state.stop_pending = true;
    return false;
}

bool motion_command_guard_init(uint32_t timeout_ms)
{
    motion_command_guard_state = (MotionCommandGuardState){0};

    const bool motor_initialized = motor_driver_init();
    const bool watchdog_initialized =
        motion_watchdog_init(timeout_ms);

    if (!motor_initialized || !watchdog_initialized)
    {
        return false;
    }

    motion_command_guard_state.initialized = true;
    return true;
}

bool motion_command_guard_stop(void)
{
    if (!motion_command_guard_state.initialized)
    {
        return false;
    }

    return motion_command_guard_try_stop();
}

bool motion_command_guard_apply(
    const MotorDriverCommand *command,
    uint32_t now_ms)
{
    if (!motion_command_guard_state.initialized ||
        command == NULL ||
        motion_command_guard_state.stop_pending)
    {
        return false;
    }

    if (!motor_driver_apply(command))
    {
        (void)motion_command_guard_try_stop();
        return false;
    }

    if ((command->left == 0) && (command->right == 0))
    {
        motion_watchdog_disarm();
        return true;
    }

    if (!motion_watchdog_refresh(now_ms))
    {
        (void)motion_command_guard_try_stop();
        return false;
    }

    return true;
}

MotionCommandGuardUpdate motion_command_guard_update(uint32_t now_ms)
{
    if (!motion_command_guard_state.initialized)
    {
        return MOTION_COMMAND_GUARD_UPDATE_NONE;
    }

    if (!motion_command_guard_state.stop_pending &&
        !motion_watchdog_expired(now_ms))
    {
        return MOTION_COMMAND_GUARD_UPDATE_NONE;
    }

    if (motion_command_guard_try_stop())
    {
        return MOTION_COMMAND_GUARD_UPDATE_STOPPED;
    }

    return MOTION_COMMAND_GUARD_UPDATE_STOP_FAILED;
}