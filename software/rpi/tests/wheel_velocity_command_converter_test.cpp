#include <gtest/gtest.h>
#include <limits>
#include "wheel_velocity_command_converter.h"

TEST(WheelVelocityCommandConverterTest, ConvertsMetersPerSecondToMillimetersPerSecond)
{
    const WheelVelocities velocities{
        .left_mps = 0.300,
        .right_mps = -0.125
    };

    WheelVelocityCommand command{};

    const auto result =
        to_wheel_velocity_command(
            velocities,
            command
        );

    EXPECT_EQ(
        result,
        WheelVelocityCommandConversionResult::Success
    );

    EXPECT_EQ(command.left_velocity_mm_s, 300);
    EXPECT_EQ(command.right_velocity_mm_s, -125);
}

TEST(WheelVelocityCommandConverterTest, RoundsToNearestMillimeterPerSecond)
{
    const WheelVelocities velocities{
        .left_mps = 0.1235,
        .right_mps = -0.1235
    };

    WheelVelocityCommand command{};

    const auto result =
        to_wheel_velocity_command(
            velocities,
            command
        );

    EXPECT_EQ(
        result,
        WheelVelocityCommandConversionResult::Success
    );

    EXPECT_EQ(command.left_velocity_mm_s, 124);
    EXPECT_EQ(command.right_velocity_mm_s, -124);
}

TEST(WheelVelocityCommandConverterTest, DoesNotModifyCommandWhenVelocityIsInvalid)
{
    const WheelVelocities velocities{
        .left_mps = 0.250,
        .right_mps = std::numeric_limits<double>::quiet_NaN()
    };

    WheelVelocityCommand command{
        .left_velocity_mm_s = 111,
        .right_velocity_mm_s = -222
    };

    const auto result =
        to_wheel_velocity_command(
            velocities,
            command
        );

    EXPECT_EQ(
        result,
        WheelVelocityCommandConversionResult::InvalidVelocity
    );

    EXPECT_EQ(command.left_velocity_mm_s, 111);
    EXPECT_EQ(command.right_velocity_mm_s, -222);
}

TEST(WheelVelocityCommandConverterTest, DoesNotModifyCommandWhenVelocityIsOutOfRange)
{
    const WheelVelocities velocities{
        .left_mps = 0.250,
        .right_mps = 32.768
    };

    WheelVelocityCommand command{
        .left_velocity_mm_s = 111,
        .right_velocity_mm_s = -222
    };

    const auto result =
        to_wheel_velocity_command(
            velocities,
            command
        );

    EXPECT_EQ(
        result,
        WheelVelocityCommandConversionResult::OutOfRange
    );

    EXPECT_EQ(command.left_velocity_mm_s, 111);
    EXPECT_EQ(command.right_velocity_mm_s, -222);
}

TEST(WheelVelocityCommandConverterTest, RejectsFiniteVelocityWhenScalingOverflows)
{
    const WheelVelocities velocities{
        .left_mps = std::numeric_limits<double>::max(),
        .right_mps = 0.0
    };

    WheelVelocityCommand command{
        .left_velocity_mm_s = 111,
        .right_velocity_mm_s = -222
    };

    const auto result =
        to_wheel_velocity_command(
            velocities,
            command
        );

    EXPECT_EQ(
        result,
        WheelVelocityCommandConversionResult::OutOfRange
    );

    EXPECT_EQ(command.left_velocity_mm_s, 111);
    EXPECT_EQ(command.right_velocity_mm_s, -222);
}

TEST(WheelVelocityCommandConverterTest, AcceptsExactInt16Limits)
{
    const WheelVelocities velocities{
        .left_mps = -32.768,
        .right_mps = 32.767
    };

    WheelVelocityCommand command{};

    const auto result =
        to_wheel_velocity_command(
            velocities,
            command
        );

    EXPECT_EQ(
        result,
        WheelVelocityCommandConversionResult::Success
    );

    EXPECT_EQ(command.left_velocity_mm_s, INT16_MIN);
    EXPECT_EQ(command.right_velocity_mm_s, INT16_MAX);
}

TEST(WheelVelocityCommandConverterTest, HandlesRoundingAndInfinityBoundaries)
{
    struct TestCase
    {
        double velocity_mps;
        WheelVelocityCommandConversionResult expected_result;
        int16_t expected_left_mm_s;
    };

    const TestCase test_cases[]{
        { 32.7674, WheelVelocityCommandConversionResult::Success,    32767 },
        { 32.7675, WheelVelocityCommandConversionResult::OutOfRange, 0 },
        {-32.7684, WheelVelocityCommandConversionResult::Success,   -32768 },
        {-32.7685, WheelVelocityCommandConversionResult::OutOfRange, 0 },
        { std::numeric_limits<double>::infinity(),
          WheelVelocityCommandConversionResult::InvalidVelocity, 0 },
        {-std::numeric_limits<double>::infinity(),
          WheelVelocityCommandConversionResult::InvalidVelocity, 0 }
    };

    for (const auto& test_case : test_cases)
    {
        SCOPED_TRACE(test_case.velocity_mps);

        const WheelVelocities velocities{
            .left_mps = test_case.velocity_mps,
            .right_mps = 0.0
        };

        WheelVelocityCommand command{
            .left_velocity_mm_s = 111,
            .right_velocity_mm_s = -222
        };

        const auto result =
            to_wheel_velocity_command(
                velocities,
                command
            );

        EXPECT_EQ(result, test_case.expected_result);

        if (result == WheelVelocityCommandConversionResult::Success)
        {
            EXPECT_EQ(
                command.left_velocity_mm_s,
                test_case.expected_left_mm_s
            );

            EXPECT_EQ(command.right_velocity_mm_s, 0);
        }
        else
        {
            EXPECT_EQ(command.left_velocity_mm_s, 111);
            EXPECT_EQ(command.right_velocity_mm_s, -222);
        }
    }
}