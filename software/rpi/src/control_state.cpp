#include "control_state.h"

ControlAuthority ControlState::authority() const
{
    return authority_;
}

ControlRequestResult ControlState::request_manual_control()
{
    if (authority_ == ControlAuthority::Manual)
        return ControlRequestResult::AlreadyActive;

    authority_ = ControlAuthority::Manual;
    return ControlRequestResult::Accepted;
}

ControlRequestResult ControlState::request_autonomous_control()
{
    if (authority_ == ControlAuthority::Autonomous)
        return ControlRequestResult::AlreadyActive;

    if (authority_ == ControlAuthority::Manual)
        return ControlRequestResult::RejectedManualActive;

    authority_ = ControlAuthority::Autonomous;
    return ControlRequestResult::Accepted;
}

ControlRequestResult ControlState::request_release_control()
{
    if (authority_ == ControlAuthority::None)
        return ControlRequestResult::AlreadyReleased;

    authority_ = ControlAuthority::None;
    return ControlRequestResult::Accepted;
}