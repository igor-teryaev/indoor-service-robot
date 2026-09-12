#pragma once

#include <chrono>

class MotionWatchdog
{
public:
    using Clock = std::chrono::steady_clock;

    explicit MotionWatchdog(
        std::chrono::milliseconds timeout);

    void refresh(Clock::time_point now);
    void disarm();

    [[nodiscard]] bool armed() const;

    [[nodiscard]] bool expired(
        Clock::time_point now) const;

private:
    std::chrono::milliseconds timeout_;
    Clock::time_point last_refresh_{};
    bool armed_ = false;
};