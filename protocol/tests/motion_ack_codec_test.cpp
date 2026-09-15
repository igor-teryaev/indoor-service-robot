#include <gtest/gtest.h>
#include "motion_ack_codec.h"

TEST(MotionAckCodecTest, EncodesAckPayload)
{
    const MotionAckPayload payload{
        .status = MOTION_ACK_ACCEPTED
    };

    uint8_t output{};
    motion_ack_encode(&payload, &output);

    const uint8_t expected = 0x00U;
    EXPECT_EQ(output, expected);
}

TEST(MotionAckCodecTest, DecodesAckPayload)
{
    const uint8_t input = 0x00U;
    MotionAckPayload payload{};

    motion_ack_decode(&input, &payload);

    EXPECT_EQ(payload.status, MOTION_ACK_ACCEPTED);
}