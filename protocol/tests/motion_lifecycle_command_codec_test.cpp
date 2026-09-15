#include <gtest/gtest.h>
#include <array>
#include "motion_lifecycle_command_codec.h"

TEST(MotionLifecycleCommandCodecTest, EncodesPayloadInBigEndian)
{
    const MotionLifecycleCommandPayload payload{
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(0x12345678)
    };

    std::array<uint8_t, MOTION_LIFECYCLE_COMMAND_WIRE_SIZE> output{};

    motion_lifecycle_command_encode(&payload, output.data());

    const std::array<uint8_t, MOTION_LIFECYCLE_COMMAND_WIRE_SIZE> expected{
        0x01, 0x12, 0x34, 0x56, 0x78
    };
    EXPECT_EQ(output, expected);
}

TEST(MotionLifecycleCommandCodecTest, DecodesPayloadInBigEndian)
{
    const std::array<uint8_t, MOTION_LIFECYCLE_COMMAND_WIRE_SIZE> input{
        0x01, 0x12, 0x34, 0x56, 0x78
    };

    MotionLifecycleCommandPayload payload{};

    motion_lifecycle_command_decode(input.data(), &payload);

    EXPECT_EQ(payload.command, MOTION_LIFECYCLE_COMMAND_START_SESSION);
    EXPECT_EQ(payload.motion_session_id, UINT32_C(0x12345678));
}

TEST(MotionLifecycleCommandCodecTest, EncodesEndSessionWithMaxSessionId)
{
    const MotionLifecycleCommandPayload payload{
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_MAX
    };

    std::array<uint8_t, MOTION_LIFECYCLE_COMMAND_WIRE_SIZE> output{};

    motion_lifecycle_command_encode(&payload, output.data());

    const std::array<uint8_t, MOTION_LIFECYCLE_COMMAND_WIRE_SIZE> expected{
        0x02, 0xFF, 0xFF, 0xFF, 0xFF
    };
    EXPECT_EQ(output, expected);
}