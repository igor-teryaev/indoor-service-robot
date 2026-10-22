#pragma once

#include <cstdint>

struct ManualWheelDemand
{
    double left;
    double right;
};

ManualWheelDemand mix_manual_sticks(double forward, double steering);

double normalize_manual_stick(std::uint16_t value);