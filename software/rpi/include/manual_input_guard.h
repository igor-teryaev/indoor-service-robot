#pragma once

#include <chrono>

#include "crsf_channels.h"

class ManualInputGuard
{
public:
    using Clock = std::chrono::steady_clock;

    void on_channels(
        const CrsfChannels& channels,
        Clock::time_point now);
    bool poll(Clock::time_point now);
    void reset();
private:
    enum class State
    {
        NeedsRearm,
        Ready,
        Enabled
    };

    State state_ = State::NeedsRearm;
    bool has_channels_ = false;
    Clock::time_point last_channels_at_{};

    static constexpr std::chrono::milliseconds timeout_{200};
};