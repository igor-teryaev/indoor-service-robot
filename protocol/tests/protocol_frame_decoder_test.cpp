#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "protocol_frame_decoder.h"
#include "protocol_message_type.h"

namespace
{

constexpr std::array<uint8_t, 15U> KNOWN_FRAME{
    0xA5U, 0x5AU,
    0x01U,
    0x03U,
    0x12U, 0x34U,
    0x00U, 0x05U,
    0x01U, 0x12U, 0x34U, 0x56U, 0x78U,
    0x3CU, 0xDDU
};

void expect_known_frame(const ProtocolFrame* frame)
{
    ASSERT_NE(frame, nullptr);

    EXPECT_EQ(
        frame->message_type,
        PROTOCOL_MESSAGE_TYPE_HEARTBEAT
    );

    EXPECT_EQ(frame->sequence, 0x1234U);
    EXPECT_EQ(frame->payload_length, 5U);

    const std::array<uint8_t, 5U> expected_payload{
        0x01U, 0x12U, 0x34U, 0x56U, 0x78U
    };

    for (size_t i = 0U; i < expected_payload.size(); ++i)
    {
        EXPECT_EQ(frame->payload[i], expected_payload[i]);
    }
}

}

TEST(ProtocolFrameDecoderTest, DecodesKnownFrameByteByByte)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U; i < KNOWN_FRAME.size(); ++i)
    {
        const bool complete =
            protocol_frame_decoder_feed_byte(
                &decoder,
                KNOWN_FRAME[i],
                &frame
            );

        if (i < KNOWN_FRAME.size() - 1U)
        {
            EXPECT_FALSE(complete);
            EXPECT_EQ(frame, nullptr);
        }
        else
        {
            EXPECT_TRUE(complete);
            expect_known_frame(frame);
        }
    }

    EXPECT_EQ(decoder.valid_frame_count, 1U);
    EXPECT_EQ(decoder.crc_error_count, 0U);
    EXPECT_EQ(decoder.format_error_count, 0U);
}

TEST(ProtocolFrameDecoderTest, RecoversFromNoiseAndOverlappingMagic)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    const std::array<uint8_t, 3U> prefix{
        0x00U, 0x7FU, 0xA5U
    };

    protocol_frame_decoder_init(&decoder);

    for (const uint8_t byte : prefix)
    {
        EXPECT_FALSE(
            protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame
            )
        );
    }

    /*
     * Stream is now:
     *
     * 00 7F A5 A5 5A ...
     *
     * The second A5 must become the new MAGIC_0.
     */
    for (const uint8_t byte : KNOWN_FRAME)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame))
        {
            expect_known_frame(frame);
        }
    }

    EXPECT_EQ(decoder.valid_frame_count, 1U);
}

TEST(ProtocolFrameDecoderTest, DecodesFrameSplitBetweenBlocks)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    constexpr size_t SPLIT = 7U;

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U; i < SPLIT; ++i)
    {
        EXPECT_FALSE(
            protocol_frame_decoder_feed_byte(
                &decoder,
                KNOWN_FRAME[i],
                &frame
            )
        );
    }

    EXPECT_EQ(decoder.valid_frame_count, 0U);

    for (size_t i = SPLIT; i < KNOWN_FRAME.size(); ++i)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                KNOWN_FRAME[i],
                &frame))
        {
            expect_known_frame(frame);
        }
    }

    EXPECT_EQ(decoder.valid_frame_count, 1U);
}

TEST(ProtocolFrameDecoderTest, DecodesMultipleFramesInSequence)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;
    uint32_t completed = 0U;

    protocol_frame_decoder_init(&decoder);

    for (uint32_t repetition = 0U;
         repetition < 2U;
         ++repetition)
    {
        for (const uint8_t byte : KNOWN_FRAME)
        {
            if (protocol_frame_decoder_feed_byte(
                    &decoder,
                    byte,
                    &frame))
            {
                expect_known_frame(frame);
                ++completed;
            }
        }
    }

    EXPECT_EQ(completed, 2U);
    EXPECT_EQ(decoder.valid_frame_count, 2U);
}

TEST(ProtocolFrameDecoderTest, RecoversAfterCrcError)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    auto corrupted = KNOWN_FRAME;
    corrupted[PROTOCOL_FRAME_PAYLOAD_OFFSET] ^= 0x01U;

    protocol_frame_decoder_init(&decoder);

    for (const uint8_t byte : corrupted)
    {
        EXPECT_FALSE(
            protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame
            )
        );
    }

    EXPECT_EQ(decoder.crc_error_count, 1U);
    EXPECT_EQ(decoder.valid_frame_count, 0U);

    for (const uint8_t byte : KNOWN_FRAME)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame))
        {
            expect_known_frame(frame);
        }
    }

    EXPECT_EQ(decoder.valid_frame_count, 1U);
}

TEST(ProtocolFrameDecoderTest, RecoversAfterInvalidVersion)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    const std::array<uint8_t, 3U> invalid_prefix{
        PROTOCOL_FRAME_MAGIC_0,
        PROTOCOL_FRAME_MAGIC_1,
        0x02U
    };

    protocol_frame_decoder_init(&decoder);

    for (const uint8_t byte : invalid_prefix)
    {
        EXPECT_FALSE(
            protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame
            )
        );
    }

    EXPECT_EQ(decoder.format_error_count, 1U);

    for (const uint8_t byte : KNOWN_FRAME)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame))
        {
            expect_known_frame(frame);
        }
    }

    EXPECT_EQ(decoder.valid_frame_count, 1U);
}

TEST(ProtocolFrameDecoderTest, RecoversAfterOversizedPayloadLength)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    const std::array<uint8_t, 8U> invalid_header{
        PROTOCOL_FRAME_MAGIC_0,
        PROTOCOL_FRAME_MAGIC_1,
        PROTOCOL_FRAME_VERSION,
        PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        0x00U, 0x01U,
        0x00U, 0x41U
    };

    protocol_frame_decoder_init(&decoder);

    for (const uint8_t byte : invalid_header)
    {
        EXPECT_FALSE(
            protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame
            )
        );
    }

    EXPECT_EQ(decoder.format_error_count, 1U);

    for (const uint8_t byte : KNOWN_FRAME)
    {
        if (protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame))
        {
            expect_known_frame(frame);
        }
    }

    EXPECT_EQ(decoder.valid_frame_count, 1U);
}

TEST(ProtocolFrameDecoderTest, ResetPreservesDiagnostics)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    auto corrupted = KNOWN_FRAME;
    corrupted[PROTOCOL_FRAME_PAYLOAD_OFFSET] ^= 0x01U;

    protocol_frame_decoder_init(&decoder);

    for (const uint8_t byte : corrupted)
    {
        EXPECT_FALSE(
            protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &frame
            )
        );
    }

    ASSERT_EQ(decoder.crc_error_count, 1U);

    protocol_frame_decoder_reset(&decoder);

    EXPECT_EQ(
        decoder.state,
        PROTOCOL_DECODER_WAIT_MAGIC_0
    );

    EXPECT_EQ(decoder.crc_error_count, 1U);
    EXPECT_EQ(decoder.valid_frame_count, 0U);
}

TEST(ProtocolFrameDecoderTest, HandlesInvalidArguments)
{
    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    protocol_frame_decoder_init(nullptr);
    protocol_frame_decoder_reset(nullptr);

    protocol_frame_decoder_init(&decoder);

    EXPECT_FALSE(
        protocol_frame_decoder_feed_byte(
            nullptr,
            0U,
            &frame
        )
    );

    EXPECT_FALSE(
        protocol_frame_decoder_feed_byte(
            &decoder,
            0U,
            nullptr
        )
    );
}

TEST(ProtocolFrameDecoderTest, DecodesStructurallyValidZeroLengthPayload)
{
    /*
     * Frame decoder validates framing, not message-specific
     * payload semantics.
     */
    const std::array<uint8_t, 10U> zero_payload_frame{
        0xA5U, 0x5AU,
        0x01U,
        0x03U,
        0x00U, 0x01U,
        0x00U, 0x00U,
        0x92U, 0x52U
    };

    ProtocolFrameDecoder decoder{};
    const ProtocolFrame* frame = nullptr;

    protocol_frame_decoder_init(&decoder);

    for (size_t i = 0U;
         i < zero_payload_frame.size();
         ++i)
    {
        const bool complete =
            protocol_frame_decoder_feed_byte(
                &decoder,
                zero_payload_frame[i],
                &frame
            );

        if (i < zero_payload_frame.size() - 1U)
        {
            EXPECT_FALSE(complete);
        }
        else
        {
            EXPECT_TRUE(complete);
            ASSERT_NE(frame, nullptr);

            EXPECT_EQ(
                frame->message_type,
                PROTOCOL_MESSAGE_TYPE_HEARTBEAT
            );

            EXPECT_EQ(frame->sequence, 1U);
            EXPECT_EQ(frame->payload_length, 0U);
        }
    }

    EXPECT_EQ(decoder.valid_frame_count, 1U);
    EXPECT_EQ(decoder.crc_error_count, 0U);
}