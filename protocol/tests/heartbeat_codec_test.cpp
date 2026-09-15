#include <gtest/gtest.h>

#include <array>

#include "heartbeat_codec.h"

TEST(HeartbeatCodecTest, EncodesSynchronizedHeartbeatInBigEndian)
{
    const HeartbeatPayload payload{
        .link_state = LINK_STATE_SYNCHRONIZED,
        .uptime_ms = UINT32_C(0x12345678)
    };

    std::array<uint8_t, HEARTBEAT_WIRE_SIZE> output{};

    heartbeat_encode(&payload, output.data());

    const std::array<uint8_t, HEARTBEAT_WIRE_SIZE> expected{
        0x01, 0x12, 0x34, 0x56, 0x78
    };

    EXPECT_EQ(output, expected);
}

TEST(HeartbeatCodecTest, DecodesSynchronizedHeartbeatInBigEndian)
{
    const std::array<uint8_t, HEARTBEAT_WIRE_SIZE> input{
        0x01, 0x12, 0x34, 0x56, 0x78
    };

    HeartbeatPayload payload{};

    heartbeat_decode(input.data(), &payload);

    EXPECT_EQ(payload.link_state, LINK_STATE_SYNCHRONIZED);
    EXPECT_EQ(payload.uptime_ms, UINT32_C(0x12345678));
}

TEST(HeartbeatCodecTest, EncodesUnsynchronizedHeartbeatWithMaxUptime)
{
    const HeartbeatPayload payload{
        .link_state = LINK_STATE_UNSYNCHRONIZED,
        .uptime_ms = UINT32_MAX
    };

    std::array<uint8_t, HEARTBEAT_WIRE_SIZE> output{};

    heartbeat_encode(&payload, output.data());

    const std::array<uint8_t, HEARTBEAT_WIRE_SIZE> expected{
        0x00, 0xFF, 0xFF, 0xFF, 0xFF
    };

    EXPECT_EQ(output, expected);
}