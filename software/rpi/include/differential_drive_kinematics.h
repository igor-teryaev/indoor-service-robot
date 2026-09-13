#pragma once

#include "motion_command.h"
#include "wheel_velocities.h"
#include "robot_geometry.h"

class DifferentialDriveKinematics
{
public:
    explicit DifferentialDriveKinematics(const RobotGeometry& geometry);

    [[nodiscard]] WheelVelocities to_wheel_velocities(
        const MotionCommand& command) const;

private:
    double track_width_m_;
};