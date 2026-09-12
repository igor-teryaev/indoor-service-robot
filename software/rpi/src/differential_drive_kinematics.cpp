#include "differential_drive_kinematics.h"

#include <cmath>
#include <stdexcept>

DifferentialDriveKinematics::DifferentialDriveKinematics(
    double track_width_m)
    : track_width_m_(track_width_m)
{
    if (!std::isfinite(track_width_m_) ||
        track_width_m_ <= 0.0)
    {
        throw std::invalid_argument(
            "track width must be finite and greater than zero"
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