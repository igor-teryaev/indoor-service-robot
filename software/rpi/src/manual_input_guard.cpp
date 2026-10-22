#include "manual_input_guard.h"

#include "manual_control_config.h"

void ManualInputGuard::reset()
{
    state_ = State::NeedsRearm;
    has_channels_ = false;
    last_channels_at_ = {};
}

bool ManualInputGuard::poll(Clock::time_point now)
{
    if (!has_channels_)
    {
        return false;
    }

    if (now - last_channels_at_ >= timeout_)
    {
        state_ = State::NeedsRearm;
    }

    return state_ == State::Enabled;
}

void ManualInputGuard::on_channels(const CrsfChannels& channels, Clock::time_point now)
{
    (void)poll(now);
    last_channels_at_ = now;
    has_channels_ = true;

    const auto sa = channels.values[manual_control_config::enable_channel];
    const bool sa_enabled = sa >= manual_control_config::enable_high_min;
    const bool sa_disabled = sa <= manual_control_config::enable_low_max;

    const bool axes_neutral = manual_control_config::is_neutral(
                            channels.values[manual_control_config::forward_channel]) &&
                        manual_control_config::is_neutral(
                                    channels.values[manual_control_config::steering_channel]);
    switch (state_)
    {
        case State::NeedsRearm:
            if (sa_disabled && axes_neutral)
            {
                state_ = State::Ready;
            }
            break;

        case State::Ready:
            if (sa_enabled && axes_neutral)
            {
                state_ = State::Enabled;
            }
            else if (!(sa_disabled && axes_neutral))
            {
                state_ = State::NeedsRearm;
            }
            break;

        case State::Enabled:
            if (!sa_enabled)
            {
                state_ = (sa_disabled && axes_neutral)
                    ? State::Ready
                    : State::NeedsRearm;
            }
            break;

        default:
            state_ = State::NeedsRearm;
            break;
    }
}