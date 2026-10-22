#pragma once

#include <cstdint>
#include <cstddef>

namespace manual_control_config
{
    inline constexpr std::size_t steering_channel = 0U;
    inline constexpr std::size_t forward_channel = 1U;
    inline constexpr std::size_t enable_channel = 4U;

    inline constexpr std::uint16_t stick_center = 992U;
    inline constexpr std::uint16_t neutral_tolerance = 25U;
    inline constexpr std::uint16_t stick_min = 174U;
    inline constexpr std::uint16_t stick_max = 1811U;

    inline constexpr std::uint16_t enable_low_max = 400U;
    inline constexpr std::uint16_t enable_high_min = 1600U;

    constexpr bool is_neutral(std::uint16_t value)
    {
        return value >= stick_center - neutral_tolerance &&
               value <= stick_center + neutral_tolerance;
    }
}
