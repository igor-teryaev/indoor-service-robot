#pragma once

#include "control_state.h"
#include "i_motor_controller.h"
#include "safety_state.h"
#include "motion_command.h"
#include "motion_watchdog.h"
#include "differential_drive_kinematics.h"

enum class ControlTransitionResult
{
    Accepted,
    AlreadyActive,
    AlreadyReleased,
    MotorStopFailed,
    InternalStateError
};

enum class MotionTickResult
{
    NoAction,
    TimeoutStopped,
    MotorStopFailed
};

enum class SafetyActionResult
{
    Updated,
    AlreadyActive,
    AlreadyClear,
    MotorStopFailed
};

class MotionCoordinator
{
public:
    MotionCoordinator(
        ControlState& control_state,
        SafetyState& safety_state,
        IMotorController& motor_controller,
        MotionWatchdog& motion_watchdog,
        DifferentialDriveKinematics& kinematics);

    [[nodiscard]] ControlTransitionResult stop_and_release_control();
    [[nodiscard]] ControlTransitionResult request_manual_control();
    [[nodiscard]] ControlTransitionResult request_autonomous_control();
    [[nodiscard]] MotionCommandResult request_motion(
        ControlAuthority requester,
        const MotionCommand& command,
        MotionWatchdog::Clock::time_point now);
    [[nodiscard]] MotionTickResult tick(
        MotionWatchdog::Clock::time_point now);

    [[nodiscard]] SafetyActionResult report_estop();
    [[nodiscard]] SafetyActionResult report_hardware_fault();

    [[nodiscard]] SafetyActionResult clear_estop();
    [[nodiscard]] SafetyActionResult clear_hardware_fault();
private:
    ControlState& control_state_;
    SafetyState& safety_state_;
    IMotorController& motor_controller_;
    MotionWatchdog& motion_watchdog_;
    DifferentialDriveKinematics& kinematics_;
};
