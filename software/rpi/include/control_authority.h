#pragma once

#include <string_view>

enum class ControlAuthority
{
    None,
    Autonomous,
    Manual
};

constexpr std::string_view control_authority_to_string(
    ControlAuthority value)
{
    switch (value)
    {
    case ControlAuthority::None:
        return "None";

    case ControlAuthority::Autonomous:
        return "Autonomous";

    case ControlAuthority::Manual:
        return "Manual";
    }

    return "Unknown";
}