#pragma once

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
};