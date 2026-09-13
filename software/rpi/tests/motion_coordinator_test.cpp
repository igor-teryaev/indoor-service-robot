#include <gtest/gtest.h>
#include <limits>

#include "motion_coordinator.h"
#include "fake_motor_controller.h"
#include "motion_config.h"

namespace
{
    constexpr RobotGeometry TEST_GEOMETRY{
        .track_width_mm = 400,
        .wheel_diameter_mm = 100
    };
}

TEST(MotionCoordinatorTest, StopsMotorsAndReleasesManualControl)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
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
        motor_controller,
        motion_watchdog,
        kinematics
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
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics);

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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller(
        MotorCommandResult::Failed
);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics);

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
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};

    ASSERT_EQ(
        control_state.request_autonomous_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};

    ASSERT_EQ(
        control_state.request_autonomous_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller(
        MotorCommandResult::Success);

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller(
        MotorCommandResult::Failed);

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller;

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_autonomous_control(),
        ControlRequestResult::Accepted);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    FakeMotorController motor_controller;
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics);

    constexpr MotionCommand command{.linear_velocity_mps = 0.5, .angular_velocity_radps = 0.0};
    const auto now = MotionWatchdog::Clock::time_point{};

    EXPECT_EQ(coordinator.request_motion(
        control_state.authority(),
        command, now),
        MotionCommandResult::RejectedWrongAuthority);
    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_wheel_velocities_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, RejectsAutonomousMotionWhileManualControlIsActive)
{
    ControlState control_state;
    SafetyState safety_state;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted);

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    constexpr MotionCommand command{.linear_velocity_mps = 0.5, .angular_velocity_radps = 0.0};
    const auto now = MotionWatchdog::Clock::time_point{};
    EXPECT_EQ(coordinator.request_motion(
        ControlAuthority::Autonomous,
        command, now),
        MotionCommandResult::RejectedWrongAuthority);
    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_wheel_velocities_called());
    EXPECT_FALSE(safety_state.hardware_fault_active());
    EXPECT_EQ(control_state.authority(), ControlAuthority::Manual);
}

TEST(MotionCoordinatorTest, RejectsMotionWithNaN)
{
    ControlState control_state;
    SafetyState safety_state;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    constexpr MotionCommand command{
        .linear_velocity_mps = std::numeric_limits<double>::quiet_NaN(),
        .angular_velocity_radps = 0.0
    };
    const auto now = MotionWatchdog::Clock::time_point{};
    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command, now
        ),
        MotionCommandResult::InvalidCommand
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_wheel_velocities_called());
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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = std::numeric_limits<double>::infinity()
    };
    const auto now = MotionWatchdog::Clock::time_point{};
    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command, now
        ),
        MotionCommandResult::InvalidCommand
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_wheel_velocities_called());
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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
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
        motor_controller,
        motion_watchdog,
        kinematics
    );

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.0,
        .angular_velocity_radps = 0.0
    };
    const auto now = MotionWatchdog::Clock::time_point{};
    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command, now
        ),
        MotionCommandResult::Accepted
    );

    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_wheel_velocities_called());

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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
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
        motor_controller,
        motion_watchdog,
        kinematics
    );

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };
    const auto now = MotionWatchdog::Clock::time_point{};
    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command, now
        ),
        MotionCommandResult::RejectedUnsafe
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_FALSE(motor_controller.set_wheel_velocities_called());

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
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};
    FakeMotorController motor_controller;

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.7
    };
    const auto now = MotionWatchdog::Clock::time_point{};
    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command, now
        ),
        MotionCommandResult::Accepted
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_TRUE(motor_controller.set_wheel_velocities_called());

    EXPECT_NEAR(
        motor_controller.last_wheel_velocities().left_mps,
        0.36,
        1e-12
    );

    EXPECT_NEAR(
        motor_controller.last_wheel_velocities().right_mps,
        0.64,
        1e-12
    );

    EXPECT_FALSE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, ReportsHardwareFaultWhenSetMotionFails)
{
    ControlState control_state;
    SafetyState safety_state;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

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
        motor_controller,
        motion_watchdog,
        kinematics
    );

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.7
    };
    const auto now = MotionWatchdog::Clock::time_point{};
    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command, now
        ),
        MotionCommandResult::MotorCommandFailed
    );

    EXPECT_TRUE(motor_controller.set_wheel_velocities_called());
    EXPECT_TRUE(motor_controller.stop_called());

    EXPECT_TRUE(safety_state.hardware_fault_active());

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Manual
    );
}

TEST(MotionCoordinatorTest, AcceptedMotionArmsWatchdog)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.2
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            t0
        ),
        MotionCommandResult::Accepted
    );

    EXPECT_TRUE(motion_watchdog.armed());

    EXPECT_FALSE(
        motion_watchdog.expired(t0)
    );

    EXPECT_TRUE(
        motion_watchdog.expired(
            t0 + MOTION_TIMEOUT
        )
    );
}

TEST(MotionCoordinatorTest, AcceptedStopDisarmsWatchdog)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand move_command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.2
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            move_command,
            t0
        ),
        MotionCommandResult::Accepted
    );

    ASSERT_TRUE(motion_watchdog.armed());

    constexpr MotionCommand stop_command{
        .linear_velocity_mps = 0.0,
        .angular_velocity_radps = 0.0
    };

    const auto t1 =
        t0 + std::chrono::milliseconds{500};

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            stop_command,
            t1
        ),
        MotionCommandResult::Accepted
    );

    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());

    EXPECT_FALSE(
        motion_watchdog.expired(
            t1 + MOTION_TIMEOUT + std::chrono::seconds{10}
        )
    );
}

TEST(MotionCoordinatorTest, TickDoesNothingBeforeTimeout)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    motion_watchdog.refresh(t0);

    EXPECT_EQ(
        coordinator.tick(
            t0 + MOTION_TIMEOUT - std::chrono::milliseconds{1}
        ),
        MotionTickResult::NoAction
    );

    EXPECT_FALSE(motor_controller.stop_called());
    EXPECT_TRUE(motion_watchdog.armed());
    EXPECT_FALSE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, TickStopsMotionOnTimeout)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    motion_watchdog.refresh(t0);

    EXPECT_EQ(
        coordinator.tick(
            t0 + MOTION_TIMEOUT
        ),
        MotionTickResult::TimeoutStopped
    );

    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
    EXPECT_FALSE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, TickReportsHardwareFaultWhenTimeoutStopFails)
{
    ControlState control_state;
    SafetyState safety_state;

    FakeMotorController motor_controller(
        MotorCommandResult::Failed
    );

    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    motion_watchdog.refresh(t0);

    EXPECT_EQ(
        coordinator.tick(
            t0 + MOTION_TIMEOUT
        ),
        MotionTickResult::MotorStopFailed
    );

    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
    EXPECT_TRUE(safety_state.hardware_fault_active());
}

TEST(MotionCoordinatorTest, RejectedMotionDoesNotRefreshWatchdog)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    ASSERT_EQ(
        control_state.request_manual_control(),
        ControlRequestResult::Accepted
    );

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            t0
        ),
        MotionCommandResult::Accepted
    );

    const auto t1 =
        t0 + std::chrono::milliseconds{1000};

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Autonomous,
            command,
            t1
        ),
        MotionCommandResult::RejectedWrongAuthority
    );

    EXPECT_EQ(
        coordinator.tick(
            t0 + MOTION_TIMEOUT
        ),
        MotionTickResult::TimeoutStopped
    );
}

TEST(MotionCoordinatorTest, RejectsFiniteMotionThatOverflowsWheelVelocities)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    EXPECT_EQ(
        coordinator.request_manual_control(),
        ControlTransitionResult::Accepted
    );

    constexpr auto now =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps =
            std::numeric_limits<double>::max(),
        .angular_velocity_radps =
            std::numeric_limits<double>::max()
    };

    EXPECT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            now
        ),
        MotionCommandResult::InvalidCommand
    );

    EXPECT_FALSE(
        motor_controller.set_wheel_velocities_called()
    );

    EXPECT_FALSE(
        motion_watchdog.armed()
    );
}

TEST(MotionCoordinatorTest, EstopStopsActiveMotionAndDisarmsWatchdog)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    EXPECT_EQ(
        coordinator.request_manual_control(),
        ControlTransitionResult::Accepted
    );

    constexpr auto now =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            now
        ),
        MotionCommandResult::Accepted
    );

    ASSERT_TRUE(motion_watchdog.armed());

    EXPECT_EQ(
        coordinator.report_estop(),
        SafetyActionResult::Updated
    );

    EXPECT_TRUE(safety_state.estop_active());
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
}

TEST(MotionCoordinatorTest, EstopStopFailureLatchesHardwareFault)
{
    ControlState control_state;
    SafetyState safety_state;

    FakeMotorController motor_controller{
        MotorCommandResult::Failed
    };

    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    EXPECT_EQ(
        coordinator.request_manual_control(),
        ControlTransitionResult::Accepted
    );

    constexpr auto now =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            now
        ),
        MotionCommandResult::Accepted
    );

    ASSERT_TRUE(motion_watchdog.armed());

    EXPECT_EQ(
        coordinator.report_estop(),
        SafetyActionResult::MotorStopFailed
    );

    EXPECT_TRUE(safety_state.estop_active());
    EXPECT_TRUE(safety_state.hardware_fault_active());

    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
}

TEST(MotionCoordinatorTest, HardwareFaultStopsActiveMotionAndDisarmsWatchdog)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    EXPECT_EQ(
        coordinator.request_manual_control(),
        ControlTransitionResult::Accepted
    );

    constexpr auto now =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            now
        ),
        MotionCommandResult::Accepted
    );

    ASSERT_TRUE(motion_watchdog.armed());

    EXPECT_EQ(
        coordinator.report_hardware_fault(),
        SafetyActionResult::Updated
    );

    EXPECT_TRUE(safety_state.hardware_fault_active());
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
}

TEST(MotionCoordinatorTest, HardwareFaultStopFailureRemainsLatched)
{
    ControlState control_state;
    SafetyState safety_state;

    FakeMotorController motor_controller{
        MotorCommandResult::Failed
    };

    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    EXPECT_EQ(
        coordinator.request_manual_control(),
        ControlTransitionResult::Accepted
    );

    constexpr auto now =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            now
        ),
        MotionCommandResult::Accepted
    );

    ASSERT_TRUE(motion_watchdog.armed());

    EXPECT_EQ(
        coordinator.report_hardware_fault(),
        SafetyActionResult::MotorStopFailed
    );

    EXPECT_TRUE(safety_state.hardware_fault_active());
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
}

TEST(MotionCoordinatorTest, ClearEstopStopsMotorAndPreservesHardwareFault)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    (void)safety_state.report_estop();
    (void)safety_state.report_hardware_fault();

    ASSERT_TRUE(safety_state.estop_active());
    ASSERT_TRUE(safety_state.hardware_fault_active());

    EXPECT_EQ(
        coordinator.clear_estop(),
        SafetyActionResult::Updated
    );

    EXPECT_FALSE(safety_state.estop_active());
    EXPECT_TRUE(safety_state.hardware_fault_active());

    EXPECT_FALSE(safety_state.safe());
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
}

TEST(MotionCoordinatorTest, ClearEstopStopFailureKeepsEstopAndLatchesHardwareFault)
{
    ControlState control_state;
    SafetyState safety_state;

    FakeMotorController motor_controller{
        MotorCommandResult::Failed
    };

    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    (void)safety_state.report_estop();

    ASSERT_TRUE(safety_state.estop_active());
    ASSERT_FALSE(safety_state.hardware_fault_active());

    EXPECT_EQ(
        coordinator.clear_estop(),
        SafetyActionResult::MotorStopFailed
    );

    EXPECT_TRUE(safety_state.estop_active());
    EXPECT_TRUE(safety_state.hardware_fault_active());

    EXPECT_FALSE(safety_state.safe());
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
}

TEST(MotionCoordinatorTest, ClearHardwareFaultStopsMotorAndPreservesEstop)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    (void)safety_state.report_estop();
    (void)safety_state.report_hardware_fault();

    ASSERT_TRUE(safety_state.estop_active());
    ASSERT_TRUE(safety_state.hardware_fault_active());

    EXPECT_EQ(
        coordinator.clear_hardware_fault(),
        SafetyActionResult::Updated
    );

    EXPECT_TRUE(safety_state.estop_active());
    EXPECT_FALSE(safety_state.hardware_fault_active());

    EXPECT_FALSE(safety_state.safe());
    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
}

TEST(MotionCoordinatorTest, ClearHardwareFaultStopFailureKeepsHardwareFaultActive)
{
    ControlState control_state;
    SafetyState safety_state;

    FakeMotorController motor_controller{
        MotorCommandResult::Failed
    };

    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    (void)safety_state.report_hardware_fault();

    ASSERT_TRUE(safety_state.hardware_fault_active());

    EXPECT_EQ(
        coordinator.clear_hardware_fault(),
        SafetyActionResult::MotorStopFailed
    );

    EXPECT_TRUE(safety_state.hardware_fault_active());
    EXPECT_FALSE(safety_state.safe());

    EXPECT_TRUE(motor_controller.stop_called());
    EXPECT_FALSE(motion_watchdog.armed());
}

TEST(MotionCoordinatorTest, SuccessfulStopAndReleaseDisarmsWatchdog)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    EXPECT_EQ(
        coordinator.request_manual_control(),
        ControlTransitionResult::Accepted
    );

    constexpr auto now =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            now
        ),
        MotionCommandResult::Accepted
    );

    ASSERT_TRUE(motion_watchdog.armed());

    EXPECT_EQ(
        coordinator.stop_and_release_control(),
        ControlTransitionResult::Accepted
    );

    EXPECT_FALSE(motion_watchdog.armed());

    EXPECT_EQ(
        coordinator.tick(now + MOTION_TIMEOUT),
        MotionTickResult::NoAction
    );
}

TEST(MotionCoordinatorTest, ManualToAutonomousDisarmsWatchdogAfterSuccessfulStop)
{
    ControlState control_state;
    SafetyState safety_state;
    FakeMotorController motor_controller;
    MotionWatchdog motion_watchdog{MOTION_TIMEOUT};
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    MotionCoordinator coordinator(
        control_state,
        safety_state,
        motor_controller,
        motion_watchdog,
        kinematics
    );

    ASSERT_EQ(
        coordinator.request_manual_control(),
        ControlTransitionResult::Accepted
    );

    constexpr auto now =
        MotionWatchdog::Clock::time_point{};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    ASSERT_EQ(
        coordinator.request_motion(
            ControlAuthority::Manual,
            command,
            now
        ),
        MotionCommandResult::Accepted
    );

    ASSERT_TRUE(motion_watchdog.armed());

    EXPECT_EQ(
        coordinator.request_autonomous_control(),
        ControlTransitionResult::Accepted
    );

    EXPECT_EQ(
        control_state.authority(),
        ControlAuthority::Autonomous
    );

    EXPECT_FALSE(motion_watchdog.armed());

    EXPECT_EQ(
        coordinator.tick(now + MOTION_TIMEOUT),
        MotionTickResult::NoAction
    );
}