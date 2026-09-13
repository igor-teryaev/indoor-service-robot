#include "differential_drive_kinematics.h"

#include <cmath>
#include <stdexcept>

DifferentialDriveKinematics::DifferentialDriveKinematics(const RobotGeometry& geometry)
    : track_width_m_(static_cast<double>(geometry.track_width_mm) / 1000.0)
{
    if (geometry.track_width_mm == 0)
    {
        throw std::invalid_argument(
            "track width must be greater than zero"
        );
    }
}

WheelVelocities DifferentialDriveKinematics::to_wheel_velocities(
    const MotionCommand& command) const
{
    const double half_track = track_width_m_ / 2.0;

    return WheelVelocities{
        .left_mps =
            command.linear_velocity_mps -
            command.angular_velocity_radps * half_track,

        .right_mps =
            command.linear_velocity_mps +
            command.angular_velocity_radps * half_track
    };
}