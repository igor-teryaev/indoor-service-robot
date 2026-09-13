#pragma once

#include "wheel_velocities.h"
#include "wheel_velocity_command.h"

enum class WheelVelocityCommandConversionResult
{
    Success,
    InvalidVelocity,
    OutOfRange
};
/*
 * Converts wheel velocities from meters per second to signed
 * 16-bit millimeters per second.
 *
 * Conversion rules:
 * - values are rounded to the nearest mm/s;
 * - halfway cases are rounded away from zero;
 * - range validation is performed after rounding;
 * - NaN and infinity are rejected;
 * - if conversion fails, the output command is left unchanged.
 */
[[nodiscard]] WheelVelocityCommandConversionResult to_wheel_velocity_command(
    const WheelVelocities& velocities,
    WheelVelocityCommand& command);