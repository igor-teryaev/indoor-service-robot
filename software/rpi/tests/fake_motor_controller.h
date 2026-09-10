#pragma once

#include "i_motor_controller.h"

class FakeMotorController : public IMotorController
{
public:
    explicit FakeMotorController(
        MotorCommandResult stop_result = MotorCommandResult::Success)
        : stop_result_(stop_result)
    {
    }

    MotorCommandResult stop() override
    {
        stop_called_ = true;
        return stop_result_;
    }

    [[nodiscard]] bool stop_called() const
    {
        return stop_called_;
    }

private:
    MotorCommandResult stop_result_;
    bool stop_called_ = false;
};