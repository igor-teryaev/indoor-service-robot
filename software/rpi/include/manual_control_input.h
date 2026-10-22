#pragma once

#include <cstdint>

extern "C"
{
#include "wheel_velocity_command.h"
}

struct ManualWheelDemand
{
    double left;
    double right;
};

ManualWheelDemand mix_manual_sticks(double forward, double steering);

double normalize_manual_stick(std::uint16_t value);

WheelVelocityCommand manual_wheels_to_command(
    const ManualWheelDemand& demand,
    std::int16_t max_velocity_mm_s);