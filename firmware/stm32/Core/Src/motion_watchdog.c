#include "motion_watchdog.h"

typedef struct
{
    bool armed;
    bool initialized;
    uint32_t timeout_ms;
    uint32_t last_refresh_ms;
} MotionWatchdogState;

static MotionWatchdogState motion_watchdog_state;

bool motion_watchdog_init(uint32_t timeout_ms)
{
    motion_watchdog_state = (MotionWatchdogState){0};

    if (timeout_ms > 0U)
    {
        motion_watchdog_state.timeout_ms = timeout_ms;
        motion_watchdog_state.initialized = true;
        return true;
    }
    return false;
}

bool motion_watchdog_refresh(uint32_t now_ms)
{
    if (!motion_watchdog_state.initialized)
    {
        return false;
    }

    motion_watchdog_state.last_refresh_ms = now_ms;
    motion_watchdog_state.armed = true;
    return true;
}

void motion_watchdog_disarm(void)
{
    motion_watchdog_state.armed = false;
}

bool motion_watchdog_is_armed(void)
{
    return motion_watchdog_state.armed;
}

bool motion_watchdog_expired(uint32_t now_ms)
{
    if (!motion_watchdog_state.initialized || !motion_watchdog_state.armed)
    {
        return false;
    }

    return (now_ms - motion_watchdog_state.last_refresh_ms) >= motion_watchdog_state.timeout_ms;
}