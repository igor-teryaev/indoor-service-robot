#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include "protocol_frame_encoder.h"
#include "wheel_speed_telemetry_frame.h"

TEST(WheelSpeedTelemetryFrameTest, BuildsExpectedWireFrame)
{
    const WheelSpeedTelemetryPayload payload{
        .left_speed_mm_s = 300,
        .right_speed_mm_s = 125,
        .valid = true
    };

    const ProtocolFrame frame =
        wheel_speed_telemetry_frame_build(&payload, 42U);

    EXPECT_EQ(
        frame.message_type,
        PROTOCOL_MESSAGE_TYPE_WHEEL_SPEED_TELEMETRY);

    EXPECT_EQ(frame.sequence, 42U);
    EXPECT_EQ(frame.payload_length, 5U);

    std::array<uint8_t, PROTOCOL_FRAME_MAX_WIRE_SIZE> wire{};

    const size_t wire_size =
        protocol_frame_encode(&frame, wire.data());

    const std::array<uint8_t, 15> expected{
        0xA5, 0x5A,       // Preamble
        0x01,             // Version
        0x40,             // WHEEL_SPEED_TELEMETRY
        0x00, 0x2A,       // Sequence = 42
        0x00, 0x05,       // Payload length = 5
        0x01, 0x2C,       // Left speed = 300
        0x00, 0x7D,       // Right speed = 125
        0x01,             // Valid
        0x6B, 0x40        // CRC-16/CCITT-FALSE
    };

    ASSERT_EQ(wire_size, expected.size());

    EXPECT_TRUE(std::equal(
        expected.begin(),
        expected.end(),
        wire.begin()));
}