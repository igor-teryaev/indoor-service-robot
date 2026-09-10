#pragma once

#include <string_view>

#include "control_authority.h"

enum class ControlRequestResult
{
    Accepted,
    AlreadyActive,
    AlreadyReleased,
    RejectedManualActive
};

constexpr std::string_view control_request_result_to_string(
    ControlRequestResult value)
{
    switch (value)
    {
    case ControlRequestResult::Accepted:
        return "Accepted";

    case ControlRequestResult::AlreadyActive:
        return "AlreadyActive";

    case ControlRequestResult::AlreadyReleased:
        return "AlreadyReleased";

    case ControlRequestResult::RejectedManualActive:
        return "RejectedManualActive";
    }

    return "Unknown";
}

class ControlState
{
public:
    [[nodiscard]] ControlAuthority authority() const;

    [[nodiscard]] ControlRequestResult request_manual_control();
    [[nodiscard]] ControlRequestResult request_autonomous_control();
    [[nodiscard]] ControlRequestResult request_release_control();

private:
    ControlAuthority authority_ = ControlAuthority::None;
};