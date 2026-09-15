#include <gtest/gtest.h>
#include <array>
#include "motion_response_codec.h"

TEST(MotionResponseCodecTest, EncodesResponseInBigEndian)
{
    const MotionResponsePayload payload{
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(0x12345678),
        .result = MOTION_RESPONSE_OK
    };

    std::array<uint8_t, MOTION_RESPONSE_WIRE_SIZE> output{};

    motion_response_encode(&payload, output.data());

    const std::array<uint8_t, MOTION_RESPONSE_WIRE_SIZE> expected{
        0x01, 0x12, 0x34, 0x56, 0x78, 0x00
    };
    EXPECT_EQ(output, expected);
}

TEST(MotionResponseCodecTest, DecodesResponseInBigEndian)
{
    const std::array<uint8_t, MOTION_RESPONSE_WIRE_SIZE> input{
        0x01, 0x12, 0x34, 0x56, 0x78, 0x00
    };

    MotionResponsePayload payload{};

    motion_response_decode(input.data(), &payload);

    EXPECT_EQ(payload.command, MOTION_LIFECYCLE_COMMAND_START_SESSION);
    EXPECT_EQ(payload.motion_session_id, UINT32_C(0x12345678));
    EXPECT_EQ(payload.result, MOTION_RESPONSE_OK);
}

TEST(MotionResponseCodecTest, EncodesEndSessionStopFailedWithMaxSessionId)
{
    const MotionResponsePayload payload{
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_MAX,
        .result = MOTION_RESPONSE_STOP_FAILED
    };

    std::array<uint8_t, MOTION_RESPONSE_WIRE_SIZE> output{};

    motion_response_encode(&payload, output.data());

    const std::array<uint8_t, MOTION_RESPONSE_WIRE_SIZE> expected{
        0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x08
    };
    EXPECT_EQ(output, expected);
}