#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

#include "differential_drive_kinematics.h"

TEST(DifferentialDriveKinematicsTest, ConvertsStraightMotionToEqualWheelVelocities)
{
    DifferentialDriveKinematics kinematics{0.4};

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
    DifferentialDriveKinematics kinematics{0.4};

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
    DifferentialDriveKinematics kinematics{0.4};

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
    DifferentialDriveKinematics kinematics{0.4};

    constexpr MotionCommand command{
        .linear_velocity_mps = 0.5,
        .angular_velocity_radps = -1.0
    };

    const auto result =
        kinematics.to_wheel_velocities(command);

    EXPECT_NEAR(result.left_mps, 0.7, 1e-12);
    EXPECT_NEAR(result.right_mps, 0.3, 1e-12);
}

TEST(DifferentialDriveKinematicsTest, ConvertsReverseCurvedMotionCorrectly)
{
    DifferentialDriveKinematics kinematics{0.4};

    constexpr MotionCommand command{
        .linear_velocity_mps = -0.5,
        .angular_velocity_radps = 1.0
    };

    const auto result =
        kinematics.to_wheel_velocities(command);

    EXPECT_NEAR(result.left_mps, -0.7, 1e-12);
    EXPECT_NEAR(result.right_mps, -0.3, 1e-12);
}

TEST(DifferentialDriveKinematicsTest, RejectsInvalidTrackWidth)
{
    EXPECT_THROW(
        DifferentialDriveKinematics{0.0},
        std::invalid_argument
    );

    EXPECT_THROW(
        DifferentialDriveKinematics{-0.4},
        std::invalid_argument
    );

    EXPECT_THROW(
        DifferentialDriveKinematics{
            std::numeric_limits<double>::quiet_NaN()
        },
        std::invalid_argument
    );

    EXPECT_THROW(
        DifferentialDriveKinematics{
            std::numeric_limits<double>::infinity()
        },
        std::invalid_argument
    );
}