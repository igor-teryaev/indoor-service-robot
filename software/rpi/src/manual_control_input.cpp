#include "manual_control_input.h"
#include "manual_control_config.h"

#include <algorithm>
#include <cmath>

constexpr auto neutral_lower = manual_control_config::stick_center - manual_control_config::neutral_tolerance;
constexpr auto neutral_upper = manual_control_config::stick_center + manual_control_config::neutral_tolerance;

double normalize_manual_stick(std::uint16_t value)
{
    if (value <= manual_control_config::stick_min) return -1.0;
    if (value >= manual_control_config::stick_max) return 1.0;
    if (manual_control_config::is_neutral(value)) return 0.0;
    if (value > manual_control_config::stick_center)
        return (value - neutral_upper) / static_cast<double>(manual_control_config::stick_max - neutral_upper);

    return -(neutral_lower - value) / static_cast<double>(neutral_lower - manual_control_config::stick_min);

}

ManualWheelDemand mix_manual_sticks(double forward, double steering)
{
    const double left = forward + steering;
    const double right = forward - steering;
    const double scale = std::max({1.0, std::abs(left), std::abs(right)});

    return ManualWheelDemand
    {
        .left = left / scale,
        .right = right / scale
    };
}