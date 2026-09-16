#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "protocol_frame_encoder.h"
#include "protocol_message_type.h"

TEST(ProtocolFrameEncoderTest, EncodesFrameWithPayload)
{
    ProtocolFrame frame{};
    frame.message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT;
    frame.sequence = 0x1234U;
    frame.payload_length = 5U;

    frame.payload[0] = 0x01U;
    frame.payload[1] = 0x12U;
    frame.payload[2] = 0x34U;
    frame.payload[3] = 0x56U;
    frame.payload[4] = 0x78U;

    std::array<uint8_t, PROTOCOL_FRAME_MAX_WIRE_SIZE> output{};

    const size_t encoded_size =
        protocol_frame_encode(&frame, output.data());

    const std::array<uint8_t, 15U> expected{
        0xA5U, 0x5AU,
        0x01U,
        0x03U,
        0x12U, 0x34U,
        0x00U, 0x05U,
        0x01U, 0x12U, 0x34U, 0x56U, 0x78U,
        0x3CU, 0xDDU
    };

    ASSERT_EQ(encoded_size, expected.size());

    for (size_t i = 0U; i < expected.size(); ++i)
    {
        EXPECT_EQ(output[i], expected[i]);
    }
}

TEST(ProtocolFrameEncoderTest, EncodesFrameWithEmptyPayload)
{
    ProtocolFrame frame{};
    frame.message_type = PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND;
    frame.sequence = 0xABCDU;
    frame.payload_length = 0U;

    std::array<uint8_t, PROTOCOL_FRAME_MAX_WIRE_SIZE> output{};

    const size_t encoded_size =
        protocol_frame_encode(&frame, output.data());

    const std::array<uint8_t, 10U> expected{
        0xA5U, 0x5AU,
        0x01U,
        0x10U,
        0xABU, 0xCDU,
        0x00U, 0x00U,
        0xDFU, 0x25U
    };

    ASSERT_EQ(encoded_size, expected.size());

    for (size_t i = 0U; i < expected.size(); ++i)
    {
        EXPECT_EQ(output[i], expected[i]);
    }
}

TEST(ProtocolFrameEncoderTest, RejectsOversizedPayloadWithoutWritingOutput)
{
    ProtocolFrame frame{};
    frame.message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT;
    frame.sequence = 0x1234U;
    frame.payload_length =
        PROTOCOL_FRAME_MAX_PAYLOAD_SIZE + 1U;

    std::array<uint8_t, PROTOCOL_FRAME_MAX_WIRE_SIZE> output{};
    output.fill(0xAAU);

    const size_t encoded_size =
        protocol_frame_encode(&frame, output.data());

    EXPECT_EQ(encoded_size, 0U);

    for (const uint8_t byte : output)
    {
        EXPECT_EQ(byte, 0xAAU);
    }
}