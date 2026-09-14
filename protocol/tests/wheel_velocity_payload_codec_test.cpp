#include <gtest/gtest.h>
#include <array>
#include "wheel_velocity_payload_codec.h"

TEST(WheelVelocityPayloadCodecTest, EncodesPayloadInBigEndian)
{
    const WheelVelocityPayload payload{
        .motion_session_id = UINT32_C(0x12345678),
        .command = {
            .left_velocity_mm_s = 300,
            .right_velocity_mm_s = -125
        }
    };

    std::array<uint8_t, WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE> output{};
    wheel_velocity_payload_encode(&payload, output.data());

    const std::array<uint8_t, WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE> expected{
        0x12, 0x34, 0x56, 0x78,
        0x01, 0x2C,
        0xFF, 0x83
    };
    EXPECT_EQ(output, expected);
}

TEST(WheelVelocityPayloadCodecTest, DecodesPayloadInBigEndian)
{
    const uint8_t input[]{
        0x12, 0x34, 0x56, 0x78,
        0x01, 0x2C,
        0xFF, 0x83};
    WheelVelocityPayload payload{};

    wheel_velocity_payload_decode(input, &payload);
    EXPECT_EQ(payload.motion_session_id, UINT32_C(0x12345678));
    EXPECT_EQ(payload.command.left_velocity_mm_s, 300);
    EXPECT_EQ(payload.command.right_velocity_mm_s, -125);
}

TEST(WheelVelocityPayloadCodecTest, EncodesAndDecodesBoundaryValues)
{
    const WheelVelocityPayload original{
        .motion_session_id = UINT32_MAX,
        .command = {
            .left_velocity_mm_s = INT16_MIN,
            .right_velocity_mm_s = INT16_MAX
        }
    };

    std::array<uint8_t, WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE> wire{};

    wheel_velocity_payload_encode(
        &original,
        wire.data());

    const std::array<uint8_t, WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE> expected{
        0xFF, 0xFF, 0xFF, 0xFF,
        0x80, 0x00,
        0x7F, 0xFF
    };

    EXPECT_EQ(wire, expected);

    WheelVelocityPayload decoded{};
    wheel_velocity_payload_decode(
        wire.data(),
        &decoded);

    EXPECT_EQ(decoded.motion_session_id, UINT32_MAX);
    EXPECT_EQ(decoded.command.left_velocity_mm_s, INT16_MIN);
    EXPECT_EQ(decoded.command.right_velocity_mm_s, INT16_MAX);
}