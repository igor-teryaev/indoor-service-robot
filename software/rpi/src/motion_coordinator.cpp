#include "motion_coordinator.h"

#include <csignal>

MotionCoordinator::MotionCoordinator(
    ControlState& control_state,
    SafetyState& safety_state,
    IMotorController& motor_controller)
    : control_state_(control_state),
      safety_state_(safety_state),
      motor_controller_(motor_controller)
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
