#pragma once

#include "i_motor_controller.h"

class FakeMotorController : public IMotorController
{
public:
    explicit FakeMotorController(
        MotorCommandResult stop_result = MotorCommandResult::Success,
        MotorCommandResult set_motion_result = MotorCommandResult::Success)
            : stop_result_(stop_result),
              set_motion_result_(set_motion_result)
    {}

    MotorCommandResult stop() override
    {
        stop_called_ = true;
        return stop_result_;
    }

    MotorCommandResult set_motion(
        const MotionCommand& command) override
    {
        set_motion_called_ = true;
        last_motion_ = command;

        return set_motion_result_;
    }

    [[nodiscard]] bool stop_called() const
    {
        return stop_called_;
    }

    [[nodiscard]] bool set_motion_called() const
    {
        return set_motion_called_;
    }

    [[nodiscard]] const MotionCommand& last_motion() const
    {
        return last_motion_;
    }

private:
    MotorCommandResult stop_result_;
    MotorCommandResult set_motion_result_;

    bool stop_called_ = false;
    bool set_motion_called_ = false;

    MotionCommand last_motion_{0.0, 0.0};
};