#include "motion_watchdog.h"

MotionWatchdog::MotionWatchdog(
    std::chrono::milliseconds timeout)
    : timeout_(timeout)
{
}

void MotionWatchdog::refresh(Clock::time_point now)
{
    last_refresh_ = now;
    armed_ = true;
}

void MotionWatchdog::disarm()
{
    armed_ = false;
}

bool MotionWatchdog::armed() const
{
    return armed_;
}

bool MotionWatchdog::expired(
    Clock::time_point now) const
{
    if (!armed_)
    {
        return false;
    }

    return now - last_refresh_ >= timeout_;
}
