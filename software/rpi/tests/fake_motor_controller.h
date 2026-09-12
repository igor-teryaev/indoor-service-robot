#pragma once

#include "i_motor_controller.h"

class FakeMotorController : public IMotorController
{
public:
    explicit FakeMotorController(
        MotorCommandResult stop_result = MotorCommandResult::Success,
        MotorCommandResult set_motion_result = MotorCommandResult::Success)
            : stop_result_(stop_result),
              set_wheel_velocities_result_(set_motion_result)
    {}

    MotorCommandResult stop() override
    {
        stop_called_ = true;
        return stop_result_;
    }

    MotorCommandResult set_wheel_velocities(const WheelVelocities& velocities) override
    {
        set_wheel_velocities_called_ = true;
        last_wheel_velocities_ = velocities;

        return set_wheel_velocities_result_;
    }

    [[nodiscard]] bool stop_called() const
    {
        return stop_called_;
    }

    [[nodiscard]] bool set_wheel_velocities_called() const
    {
        return set_wheel_velocities_called_;
    }

    [[nodiscard]] const WheelVelocities& last_wheel_velocities() const
    {
        return last_wheel_velocities_;
    }

private:
    MotorCommandResult stop_result_;
    MotorCommandResult set_wheel_velocities_result_;

    bool stop_called_ = false;
    bool set_wheel_velocities_called_ = false;
    WheelVelocities last_wheel_velocities_{0.0, 0.0};
};