#include <gtest/gtest.h>

#include "wheel_velocity_command_converter.h"
#include "wheel_velocity_codec.h"

TEST(WheelVelocityProtocolIntegrationTest, ConvertsAndEncodesWheelVelocities)
{
    const WheelVelocities velocities{
        .left_mps = 0.300,
        .right_mps = -0.125
    };

    WheelVelocityCommand command{};

    ASSERT_EQ(
        to_wheel_velocity_command(velocities, command),
        WheelVelocityCommandConversionResult::Success
    );

    uint8_t output[WHEEL_VELOCITY_COMMAND_WIRE_SIZE]{};

    wheel_velocity_command_encode(
        &command,
        output
    );

    EXPECT_EQ(output[0], 0x01);
    EXPECT_EQ(output[1], 0x2C);
    EXPECT_EQ(output[2], 0xFF);
    EXPECT_EQ(output[3], 0x83);
}