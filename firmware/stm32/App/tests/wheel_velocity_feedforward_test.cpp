#include <gtest/gtest.h>

extern "C"
{
#include "wheel_velocity_feedforward.h"
}

namespace
{
    constexpr WheelVelocityFeedforwardConfig CONFIG =
    {
        .max_velocity_mm_s = 400U,
        .minimum_start_command = 800U
    };
}

TEST(WheelVelocityFeedforwardTest, ConvertsZeroVelocityToZeroCommand)
{
    constexpr WheelVelocityCommand velocity =
    {
        .left_velocity_mm_s = 0,
        .right_velocity_mm_s = 0
    };

    MotorDriverCommand command = {};

    ASSERT_TRUE(
        wheel_velocity_feedforward_convert(
            &CONFIG,
            &velocity,
            &command));

    EXPECT_EQ(command.left, 0);
    EXPECT_EQ(command.right, 0);
}

TEST(WheelVelocityFeedforwardTest, MapsPositiveVelocityIntoStartupRange)
{
    constexpr WheelVelocityCommand velocity =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 200
    };

    MotorDriverCommand command = {};

    ASSERT_TRUE(
        wheel_velocity_feedforward_convert(
            &CONFIG,
            &velocity,
            &command));

    EXPECT_EQ(command.left, 850);
    EXPECT_EQ(command.right, 900);
}

TEST(WheelVelocityFeedforwardTest, PreservesNegativeDirection)
{
    constexpr WheelVelocityCommand velocity =
    {
        .left_velocity_mm_s = -100,
        .right_velocity_mm_s = -400
    };

    MotorDriverCommand command = {};

    ASSERT_TRUE(
        wheel_velocity_feedforward_convert(
            &CONFIG,
            &velocity,
            &command));

    EXPECT_EQ(command.left, -850);
    EXPECT_EQ(command.right, -1000);
}

TEST(WheelVelocityFeedforwardTest, SaturatesAboveConfiguredMaximum)
{
    constexpr WheelVelocityCommand velocity =
    {
        .left_velocity_mm_s = 1000,
        .right_velocity_mm_s = -1000
    };

    MotorDriverCommand command = {};

    ASSERT_TRUE(
        wheel_velocity_feedforward_convert(
            &CONFIG,
            &velocity,
            &command));

    EXPECT_EQ(command.left, 1000);
    EXPECT_EQ(command.right, -1000);
}

TEST(WheelVelocityFeedforwardTest, HandlesInt16MinSafely)
{
    constexpr WheelVelocityCommand velocity =
    {
        .left_velocity_mm_s = INT16_MIN,
        .right_velocity_mm_s = INT16_MAX
    };

    MotorDriverCommand command = {};

    ASSERT_TRUE(
        wheel_velocity_feedforward_convert(
            &CONFIG,
            &velocity,
            &command));

    EXPECT_EQ(command.left, -1000);
    EXPECT_EQ(command.right, 1000);
}

TEST(WheelVelocityFeedforwardTest, RejectsInvalidConfiguration)
{
    constexpr WheelVelocityFeedforwardConfig invalid_config =
    {
        .max_velocity_mm_s = 0U,
        .minimum_start_command = 800U
    };

    constexpr WheelVelocityCommand velocity =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    MotorDriverCommand command = {};

    EXPECT_FALSE(
        wheel_velocity_feedforward_convert(
            &invalid_config,
            &velocity,
            &command));
}