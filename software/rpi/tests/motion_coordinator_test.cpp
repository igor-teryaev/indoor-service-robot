#include <gtest/gtest.h>

#include "motion_coordinator.h"
#include "fake_motor_controller.h"

TEST(MotionCoordinatorTest, StopsMotorsAndReleasesManualControl)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result =
        coordinator.stop_and_release_control();

    EXPECT_EQ(
        result,
        ControlTransitionResult::Accepted
    );

    EXPECT_TRUE(
        motor_controller.stop_called()
    );

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::None
    );

    EXPECT_FALSE(
        safety_state.hardware_fault_active()
    );
}

TEST(MotionCoordinatorTest, ReportsHardwareFaultWhenMotorStopFails)
{
    ControlState control_state;
    SafetyState safety_state;

    FakeMotorController motor_controller(
        MotorCommandResult::Failed
    );

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result =
        coordinator.stop_and_release_control();

    EXPECT_EQ(
        result,
        ControlTransitionResult::MotorStopFailed
    );

    EXPECT_TRUE(
        motor_controller.stop_called()
    );

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );

    EXPECT_TRUE(
        safety_state.hardware_fault_active()
    );
}

TEST(MotionCoordinatorTest, CallsStopWhenControlAlreadyReleased)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller);

    const auto result = coordinator.stop_and_release_control();

    EXPECT_EQ(result, ControlTransitionResult::AlreadyReleased);
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_EQ(control_state.authority(), ControlAuthority::None);
    EXPECT_FALSE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, ReportsHardwareFaultWhenAlreadyReleasedStopFails)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller(
        MotorCommandResult::Failed
);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller);

    const auto result = coordinator.stop_and_release_control();

    EXPECT_EQ(result, ControlTransitionResult::MotorStopFailed);
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_EQ(control_state.authority(), ControlAuthority::None);
    EXPECT_TRUE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, StopsMotorsAndReleasesAutonomousControl)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_autonomous_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result =
        coordinator.stop_and_release_control();

    EXPECT_EQ(
        result,
        ControlTransitionResult::Accepted
    );

    EXPECT_TRUE(
        motor_controller.stop_called()
    );

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::None
    );

    EXPECT_FALSE(
        safety_state.hardware_fault_active()
    );
}

TEST(MotionCoordinatorTest, RequestsManualControlFromAutonomous)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_autonomous_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result =
        coordinator.request_manual_control();

    EXPECT_EQ(
        result,
        ControlTransitionResult::Accepted
    );

    EXPECT_FALSE(
        motor_controller.stop_called()
    );

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}

TEST(MotionCoordinatorTest, ReportsAlreadyActiveWhenManualControlAlreadyActive)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result =
        coordinator.request_manual_control();

    EXPECT_EQ(
        result,
        ControlTransitionResult::AlreadyActive
    );

    EXPECT_FALSE(
        motor_controller.stop_called()
    );

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}

TEST(MotionCoordinatorTest, RequestsManualControlFromNoAuthority)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result =
        coordinator.request_manual_control();

    EXPECT_EQ(
        result,
        ControlTransitionResult::Accepted
    );

    EXPECT_FALSE(
        motor_controller.stop_called()
    );

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}

TEST(MotionCoordinatorTest, ManualToAutonomousControlStopSuccess)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller(
        MotorCommandResult::Success);

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result = coordinator.request_autonomous_control();
    EXPECT_EQ(result, ControlTransitionResult::Accepted);
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
    EXPECT_EQ(control_state.authority(), ControlAuthority::Autonomous);
}

TEST(MotionCoordinatorTest, ManualToAutonomousControlStopFailed)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller(
        MotorCommandResult::Failed);

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result = coordinator.request_autonomous_control();
    EXPECT_EQ(result, ControlTransitionResult::MotorStopFailed);
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_TRUE(safety_state.hardware_fault_active());
    EXPECT_EQ(control_state.authority(), ControlAuthority::Manual);
}

TEST(MotionCoordinatorTest, NoneToAutonomousControl)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result = coordinator.request_autonomous_control();
    EXPECT_EQ(result, ControlTransitionResult::Accepted);
    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
    EXPECT_EQ(control_state.authority(), ControlAuthority::Autonomous);
}

TEST(MotionCoordinatorTest, AutonomousToAutonomousControl)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_autonomous_control(),
        ControlRequestResult::Accepted);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    const auto result = coordinator.request_autonomous_control();
    EXPECT_EQ(result, ControlTransitionResult::AlreadyActive);
    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
    EXPECT_EQ(control_state.authority(), ControlAuthority::Autonomous);
}