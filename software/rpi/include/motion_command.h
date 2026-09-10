#pragma once

struct MotionCommand
{
    double linear_velocity_mps;
    double angular_velocity_radps;
};

enum class MotionCommandResult
{
    Accepted,
    RejectedWrongAuthority,
    RejectedUnsafe,
    InvalidCommand,
    MotorCommandFailed
};