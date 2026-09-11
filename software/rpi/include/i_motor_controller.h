#pragma once

#include "motion_command.h"

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

    virtual MotorCommandResult set_motion(const MotionCommand& command) = 0;
};