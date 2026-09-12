#pragma once

#include "wheel_velocities.h"

enum class MotorCommandResult
{
    Success,
    Failed
};

class IMotorController
{
public:
    virtual ~IMotorController() = default;

    virtual MotorCommandResult stop() = 0;

    virtual MotorCommandResult set_wheel_velocities(const WheelVelocities& velocities) = 0;
};