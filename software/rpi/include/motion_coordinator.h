#pragma once

#include "control_state.h"
#include "i_motor_controller.h"
#include "safety_state.h"
#include "motion_command.h"

enum class ControlTransitionResult
{
    Accepted,
    AlreadyActive,
    AlreadyReleased,
    MotorStopFailed,
    InternalStateError
};

class MotionCoordinator
{
public:
    MotionCoordinator(
        ControlState& control_state,
        SafetyState& safety_state,
        IMotorController& motor_controller);
    [[nodiscard]] ControlTransitionResult stop_and_release_control();
    [[nodiscard]] ControlTransitionResult request_manual_control();
    [[nodiscard]] ControlTransitionResult request_autonomous_control();
    [[nodiscard]] MotionCommandResult request_motion(
        ControlAuthority requester,
        const MotionCommand& command);

private:
    ControlState& control_state_;
    SafetyState& safety_state_;
    IMotorController& motor_controller_;
};
