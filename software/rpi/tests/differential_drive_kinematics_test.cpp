#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

#include "differential_drive_kinematics.h"

namespace
{
    constexpr RobotGeometry TEST_GEOMETRY{
        .track_width_mm = 400,
        .wheel_diameter_mm = 100
    };
}

TEST(DifferentialDriveKinematicsTest, ConvertsStraightMotionToEqualWheelVelocities)
{
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 0.0
    };

    const auto result =
        kinematics.to_wheel_velocities(command);

    EXPECT_DOUBLE_EQ(result.left_mps, 0.5);
    EXPECT_DOUBLE_EQ(result.right_mps, 0.5);
}

TEST(DifferentialDriveKinematicsTest, ConvertsRotationInPlaceToOppositeWheelVelocities)
{
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.0,
        .angular_velocity_radps = 1.0
    };

    const auto result =
        kinematics.to_wheel_velocities(command);

    EXPECT_DOUBLE_EQ(result.left_mps, -0.2);
    EXPECT_DOUBLE_EQ(result.right_mps, 0.2);
}

TEST(DifferentialDriveKinematicsTest, ConvertsCurvedMotionToDifferentWheelVelocities)
{
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = 1.0
    };

    const auto result =
        kinematics.to_wheel_velocities(command);

    EXPECT_NEAR(result.left_mps, 0.3, 1e-12);
    EXPECT_NEAR(result.right_mps, 0.7, 1e-12);
}

TEST(DifferentialDriveKinematicsTest, ReversesWheelDifferenceForNegativeAngularVelocity)
{
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = -1.0
    };

    const auto result =
        kinematics.to_wheel_velocities(command);

    EXPECT_NEAR(result.left_mps, 0.7, 1e-12);
    EXPECT_NEAR(result.right_mps, 0.3, 1e-12);
}

TEST(DifferentialDriveKinematicsTest, RejectsZeroTrackWidth)
{
    constexpr RobotGeometry geometry{
        .track_width_mm = 0,
        .wheel_diameter_mm = 100
    };

    EXPECT_THROW(
        DifferentialDriveKinematics{geometry},
        std::invalid_argument
    );
}

TEST(DifferentialDriveKinematicsTest, ConvertsReverseCurvedMotionCorrectly)
{
    DifferentialDriveKinematics kinematics{TEST_GEOMETRY};

    constexpr MotionCommand command{
        .linear_velocity_mps = -0.5,
        .angular_velocity_radps = 1.0
    };

    const auto result =
        kinematics.to_wheel_velocities(command);

    EXPECT_NEAR(result.left_mps, -0.7, 1e-12);
    EXPECT_NEAR(result.right_mps, -0.3, 1e-12);
}
