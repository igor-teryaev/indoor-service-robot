#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "wheel_speed_telemetry_payload_codec.h"

TEST(WheelSpeedTelemetryPayloadCodecTest, EncodesPayloadInBigEndian)
{
    const WheelSpeedTelemetryPayload payload{
        .left_speed_mm_s = 300,
        .right_speed_mm_s = 125,
        .valid = true
    };

    std::array<uint8_t, WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE> wire{};

    wheel_speed_telemetry_payload_encode(&payload, wire.data());

    const std::array<uint8_t, WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE> expected{
        0x01, 0x2C,
        0x00, 0x7D,
        0x01
    };

    EXPECT_EQ(wire, expected);
}

TEST(WheelSpeedTelemetryPayloadCodecTest, DecodesPayloadInBigEndian)
{
    const uint8_t wire[]{
        0x00, 0x78,
        0x00, 0x55,
        0x00
    };

    WheelSpeedTelemetryPayload payload{};

    wheel_speed_telemetry_payload_decode(wire, &payload);

    EXPECT_EQ(payload.left_speed_mm_s, 120);
    EXPECT_EQ(payload.right_speed_mm_s, 85);
    EXPECT_FALSE(payload.valid);
}

TEST(WheelSpeedTelemetryPayloadCodecTest, RoundTripsBoundaryValues)
{
    const WheelSpeedTelemetryPayload original{
        .left_speed_mm_s = 0,
        .right_speed_mm_s = UINT16_MAX,
        .valid = true
    };

    std::array<uint8_t, WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE> wire{};

    wheel_speed_telemetry_payload_encode(&original, wire.data());

    const std::array<uint8_t, WHEEL_SPEED_TELEMETRY_PAYLOAD_WIRE_SIZE> expected{
        0x00, 0x00,
        0xFF, 0xFF,
        0x01
    };

    EXPECT_EQ(wire, expected);

    WheelSpeedTelemetryPayload decoded{};

    wheel_speed_telemetry_payload_decode(wire.data(), &decoded);

    EXPECT_EQ(decoded.left_speed_mm_s, original.left_speed_mm_s);
    EXPECT_EQ(decoded.right_speed_mm_s, original.right_speed_mm_s);
    EXPECT_EQ(decoded.valid, original.valid);
}