#include "motion_coordinator.h"

#include <csignal>
#include <cmath>

constexpr double LINEAR_STOP_EPSILON_MPS = 1e-5;
constexpr double ANGULAR_STOP_EPSILON_RADPS = 1e-5;

MotionCoordinator::MotionCoordinator(
    ControlState& control_state,
    SafetyState& safety_state,
    IMotorController& motor_controller,
    MotionWatchdog& motion_watchdog)
    : control_state_(control_state),
      safety_state_(safety_state),
      motor_controller_(motor_controller),
      motion_watchdog_(motion_watchdog)
{
}

ControlTransitionResult MotionCoordinator::request_manual_control()
{
    switch (control_state_.request_manual_control())
    {
        case ControlRequestResult::Accepted:
            return ControlTransitionResult::Accepted;
        case ControlRequestResult::AlreadyActive:
            return ControlTransitionResult::AlreadyActive;
        default:
            return ControlTransitionResult::InternalStateError;
    }
}

ControlTransitionResult MotionCoordinator::stop_and_release_control()
{
    if (motor_controller_.stop() == MotorCommandResult::Failed)
    {
        (void)safety_state_.report_hardware_fault();
        return ControlTransitionResult::MotorStopFailed;
    }

    switch (control_state_.request_release_control())
    {
        case ControlRequestResult::Accepted:
            return ControlTransitionResult::Accepted;

        case ControlRequestResult::AlreadyReleased:
            return ControlTransitionResult::AlreadyReleased;

        default:
            return ControlTransitionResult::InternalStateError;
    }
}

ControlTransitionResult MotionCoordinator::request_autonomous_control()
{
    if (control_state_.authority() == ControlAuthority::Manual)
    {
        if (motor_controller_.stop() != MotorCommandResult::Success)
        {
            (void)safety_state_.report_hardware_fault();
            return ControlTransitionResult::MotorStopFailed;
        }

        if (control_state_.request_release_control() != ControlRequestResult::Accepted)
        {
            return ControlTransitionResult::InternalStateError;
        }
    }

    switch (control_state_.request_autonomous_control())
    {
        case ControlRequestResult::Accepted:
            return ControlTransitionResult::Accepted;
        case ControlRequestResult::AlreadyActive:
            return ControlTransitionResult::AlreadyActive;
        default:
            return ControlTransitionResult::InternalStateError;
    }
}

MotionCommandResult MotionCoordinator::request_motion(
    ControlAuthority requester,
    const MotionCommand& command,
    MotionWatchdog::Clock::time_point now)
{
    if (requester == ControlAuthority::None ||
        requester != control_state_.authority())
    {
        return MotionCommandResult::RejectedWrongAuthority;
    }

    if (!std::isfinite(command.angular_velocity_radps) ||
        !std::isfinite(command.linear_velocity_mps))
    {
        return MotionCommandResult::InvalidCommand;
    }

    if (std::abs(command.linear_velocity_mps) < LINEAR_STOP_EPSILON_MPS &&
        std::abs(command.angular_velocity_radps) < ANGULAR_STOP_EPSILON_RADPS)
    {
        if (motor_controller_.stop() == MotorCommandResult::Success)
        {
            motion_watchdog_.disarm();
            return MotionCommandResult::Accepted;
        }

        safety_state_.report_hardware_fault();
        return MotionCommandResult::MotorCommandFailed;
    }

    if (!safety_state_.safe())
    {
        return MotionCommandResult::RejectedUnsafe;
    }

    if (motor_controller_.set_motion(command) == MotorCommandResult::Success)
    {
        motion_watchdog_.refresh(now);
        return MotionCommandResult::Accepted;
    }

    safety_state_.report_hardware_fault();
    (void)motor_controller_.stop();
    return MotionCommandResult::MotorCommandFailed;
}

MotionTickResult MotionCoordinator::tick(
    MotionWatchdog::Clock::time_point now)
{
    if (!motion_watchdog_.expired(now))
    {
        return MotionTickResult::NoAction;
    }

    motion_watchdog_.disarm();

    if (motor_controller_.stop() == MotorCommandResult::Success)
    {
        return MotionTickResult::TimeoutStopped;
    }

    safety_state_.report_hardware_fault();
    return MotionTickResult::MotorStopFailed;
}