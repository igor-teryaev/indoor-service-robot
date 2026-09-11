#include <gtest/gtest.h>
#include <limits>

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

TEST(MotionCoordinatorTest, RejectsMotionWhenNoAuthorityIsActive)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    constexpr MotionCommand command{.linear_velocity_mps = 0.5, .angular_velocity_radps = 0.0};

    EXPECT_EQ(coordinator.request_motion(
        control_state.authority(),
        command),
        MotionCommandResult::RejectedWrongAuthority);
    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_motion_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, RejectsAutonomousMotionWhileManualControlIsActive)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    constexpr MotionCommand command{.linear_velocity_mps = 0.5, .angular_velocity_radps = 0.0};

    EXPECT_EQ(coordinator.request_motion(
        ControlAuthority::Autonomous,
        command),
        MotionCommandResult::RejectedWrongAuthority);
    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_motion_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
    EXPECT_EQ(control_state.authority(), ControlAuthority::Manual);
}

TEST(MotionCoordinatorTest, RejectsMotionWithNaN)
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

    constexpr MotionCommand command{
        .linear_velocity_mps = std::numeric_limits<double>::quiet_NaN(),
        .angular_velocity_radps = 0.0
    };

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command
        ),
        MotionCommandResult::InvalidCommand
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_motion_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}

TEST(MotionCoordinatorTest, RejectsMotionWithInfinity)
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

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = std::numeric_limits<double>::infinity()
    };

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command
        ),
        MotionCommandResult::InvalidCommand
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_motion_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}

TEST(MotionCoordinatorTest, AllowsStopWhileUnsafe)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    ASSERT_EQ(
        safety_state.report_hardware_fault(),
        SafetyStateResult::Updated
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.0,
        .angular_velocity_radps = 0.0
    };

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command
        ),
        MotionCommandResult::Accepted
    );

    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_motion_called());

    EXPECT_TRUE(safety_state.hardware_fault_active());

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}

TEST(MotionCoordinatorTest, RejectsMotionWhileUnsafe)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    ASSERT_EQ(
        safety_state.report_hardware_fault(),
        SafetyStateResult::Updated
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller
    );

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command
        ),
        MotionCommandResult::RejectedUnsafe
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_motion_called());

    EXPECT_TRUE(safety_state.hardware_fault_active());

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}

TEST(MotionCoordinatorTest, SendsValidMotionCommandToMotorController)
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

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.7
    };

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command
        ),
        MotionCommandResult::Accepted
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_TRUE(motor_controller.set_motion_called());

    EXPECT_DOUBLE_EQ(
        motor_controller.last_motion().linear_velocity_mps,
        0.5
    );

    EXPECT_DOUBLE_EQ(
        motor_controller.last_motion().angular_velocity_radps,
        0.7
    );

    EXPECT_FALSE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, ReportsHardwareFaultWhenSetMotionFails)
{
    ControlState control_state;
    SafetyState safety_state;

    FakeMotorController motor_controller(
        MotorCommandResult::Success,
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

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.7
    };

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command
        ),
        MotionCommandResult::MotorCommandFailed
    );

    EXPECT_TRUE(motor_controller.set_motion_called());
    EXPECT_TRUE(motor_controller.stop_called());

    EXPECT_TRUE(safety_state.hardware_fault_active());

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}