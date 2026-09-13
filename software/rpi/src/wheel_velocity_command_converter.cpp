#include "wheel_velocity_command_converter.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace
{
    WheelVelocityCommandConversionResult convert_velocity(
        double velocity_mps,
        std::int16_t& velocity_mm_s)
    {
        if (!std::isfinite(velocity_mps))
        {
            return WheelVelocityCommandConversionResult::InvalidVelocity;
        }

        const double scaled = velocity_mps * 1000.0;

        if (!std::isfinite(scaled))
        {
            return WheelVelocityCommandConversionResult::OutOfRange;
        }

        const double rounded = std::round(scaled);

        if (rounded < std::numeric_limits<std::int16_t>::min() ||
            rounded > std::numeric_limits<std::int16_t>::max())
        {
            return WheelVelocityCommandConversionResult::OutOfRange;
        }

        velocity_mm_s = static_cast<std::int16_t>(rounded);

        return WheelVelocityCommandConversionResult::Success;
    }
}

WheelVelocityCommandConversionResult to_wheel_velocity_command(
    const WheelVelocities& velocities,
    WheelVelocityCommand& command)
{
    std::int16_t left_mm_s{};
    std::int16_t right_mm_s{};

    const auto left_result =
        convert_velocity(
            velocities.left_mps,
            left_mm_s);

    if (left_result != WheelVelocityCommandConversionResult::Success)
    {
        return left_result;
    }

    const auto right_result =
        convert_velocity(
            velocities.right_mps,
            right_mm_s);

    if (right_result != WheelVelocityCommandConversionResult::Success)
    {
        return right_result;
    }

    command.left_velocity_mm_s = left_mm_s;
    command.right_velocity_mm_s = right_mm_s;

    return WheelVelocityCommandConversionResult::Success;
}