#pragma once

#include "motion_command.h"
#include "wheel_velocities.h"

class DifferentialDriveKinematics
{
public:
    explicit DifferentialDriveKinematics(double track_width_m);

    [[nodiscard]] WheelVelocities to_wheel_velocities(
        const MotionCommand& command) const;

private:
    double track_width_m_;
};