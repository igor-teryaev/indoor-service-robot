#include <gtest/gtest.h>

#include <array>

#include "wheel_velocity_codec.h"

TEST(WheelVelocityCodecTest, EncodesOnlyFourBytesAtAnOffset)
{
    static_assert(WHEEL_VELOCITY_COMMAND_WIRE_SIZE == 4U);
    const WheelVelocityCommand command{
        .left_velocity_mm_s = -1,
        .right_velocity_mm_s = 0
    };
    std::array<uint8_t, 6> output{0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5};

    wheel_velocity_command_encode(&command, output.data() + 1);

    const std::array<uint8_t, 6> expected{0xA5, 0xFF, 0xFF, 0x00, 0x00, 0xA5};
    EXPECT_EQ(output, expected);
}

TEST(WheelVelocityCodecTest, DecodesZeroAndMinusOneAtAnOffset)
{
    const uint8_t input[]{0xA5, 0x00, 0x00, 0xFF, 0xFF, 0xA5};
    WheelVelocityCommand command{};

    wheel_velocity_command_decode(input + 1, &command);

    EXPECT_EQ(command.left_velocity_mm_s, 0);
    EXPECT_EQ(command.right_velocity_mm_s, -1);
}

TEST(WheelVelocityCodecTest, EncodesSignedVelocitiesInBigEndian)
{
    const WheelVelocityCommand command{
        .left_velocity_mm_s = 300,
        .right_velocity_mm_s = -125
    };

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

TEST(WheelVelocityCodecTest, DecodesSignedVelocitiesFromBigEndian)
{
    const uint8_t input[WHEEL_VELOCITY_COMMAND_WIRE_SIZE]{
        0x01, 0x2C,
        0xFF, 0x83
    };

    WheelVelocityCommand command{};

    wheel_velocity_command_decode(
        input,
        &command
    );

    EXPECT_EQ(command.left_velocity_mm_s, 300);
    EXPECT_EQ(command.right_velocity_mm_s, -125);
}

TEST(WheelVelocityCodecTest, DecodesInt16Limits)
{
    const uint8_t input[WHEEL_VELOCITY_COMMAND_WIRE_SIZE]{
        0x80, 0x00,   // -32768
        0x7F, 0xFF    //  32767
    };

    WheelVelocityCommand command{};

    wheel_velocity_command_decode(
        input,
        &command
    );

    EXPECT_EQ(command.left_velocity_mm_s, INT16_MIN);
    EXPECT_EQ(command.right_velocity_mm_s, INT16_MAX);
}

TEST(WheelVelocityCodecTest, EncodesInt16Limits)
{
    const WheelVelocityCommand command{
        .left_velocity_mm_s = INT16_MIN,
        .right_velocity_mm_s = INT16_MAX
    };

    uint8_t output[WHEEL_VELOCITY_COMMAND_WIRE_SIZE]{};

    wheel_velocity_command_encode(
        &command,
        output
    );

    EXPECT_EQ(output[0], 0x80);
    EXPECT_EQ(output[1], 0x00);
    EXPECT_EQ(output[2], 0x7F);
    EXPECT_EQ(output[3], 0xFF);
}
