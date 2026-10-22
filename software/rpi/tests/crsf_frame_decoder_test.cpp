#include <gtest/gtest.h>
#include <array>
#include "crsf_frame_decoder.h"

#include "crsf_crc8.h"

TEST(CrsfFrameDecoderTest, DecodesTwoConsecutiveFrames)
{
    constexpr std::uint8_t wire[] = {
        0xC8, 0x0C, 0x14,
        0x1B, 0x17, 0x00, 0x0C, 0x01,
        0x07, 0x03, 0x00, 0x00, 0x00,
        0x95
    };

    CrsfFrameDecoder decoder;
    CrsfFrame frame;

    for (int repetition = 0; repetition < 2; ++repetition)
    {
        for (std::size_t i = 0U; i < sizeof(wire); ++i)
        {
            ASSERT_EQ(
                decoder.feed_byte(wire[i], frame),
                i == sizeof(wire) - 1U);
        }

        EXPECT_EQ(frame.type, 0x14U);
        ASSERT_EQ(frame.payload_length, 10U);

        for (std::size_t i = 0U; i < frame.payload_length; ++i)
        {
            EXPECT_EQ(frame.payload[i], wire[i + 3U]);
        }
    }
}

TEST(CrsfFrameDecoderTest, RejectsBadCrcAndRecovers)
{
    constexpr std::array<std::uint8_t, 14> valid = {
        0xC8, 0x0C, 0x14,
        0x1B, 0x17, 0x00, 0x0C, 0x01,
        0x07, 0x03, 0x00, 0x00, 0x00,
        0x95
    };

    auto corrupted = valid;
    corrupted.back() ^= 0x01U;

    CrsfFrameDecoder decoder;
    CrsfFrame frame{};
    frame.type = 0xAAU;
    frame.payload_length = 1U;
    frame.payload[0] = 0x55U;

    for (const auto byte : corrupted)
    {
        EXPECT_FALSE(decoder.feed_byte(byte, frame));
    }

    EXPECT_EQ(frame.type, 0xAAU);
    EXPECT_EQ(frame.payload_length, 1U);
    EXPECT_EQ(frame.payload[0], 0x55U);

    for (std::size_t i = 0U; i < valid.size(); ++i)
    {
        ASSERT_EQ(
            decoder.feed_byte(valid[i], frame),
            i == valid.size() - 1U);
    }

    EXPECT_EQ(frame.type, 0x14U);
    EXPECT_EQ(frame.payload_length, 10U);
}

TEST(CrsfFrameDecoderTest, RecoversFromInvalidLengthAndDecodesEmptyPayload)
{
    CrsfFrameDecoder decoder;
    CrsfFrame frame{};

    EXPECT_FALSE(decoder.feed_byte(0xC8U, frame));
    EXPECT_FALSE(decoder.feed_byte(0x01U, frame));

    EXPECT_FALSE(decoder.feed_byte(0xC8U, frame));
    EXPECT_FALSE(decoder.feed_byte(0x02U, frame));
    EXPECT_FALSE(decoder.feed_byte(0x00U, frame));
    ASSERT_TRUE(decoder.feed_byte(0x00U, frame));

    EXPECT_EQ(frame.type, 0x00U);
    EXPECT_EQ(frame.payload_length, 0U);
}

TEST(CrsfFrameDecoderTest, Valid64byteFrameFollowedByAnotherFrame)
{
    auto my_array = []() {
        std::array<std::uint8_t, 64> arr{};

        arr[0] = 0xC8;
        arr[1] = 62;
        arr[2] = 0x16;

        // Fill the remaining 60 bytes with 0x55
        for (size_t i = 3; i < arr.size() -1U; ++i) {
            arr[i] = 0x55;
        }

        return arr;
    }();

    std::uint8_t calculated_crc = 0;
    for (std::size_t i = 2; i <= my_array.size() - 2; ++i) {
        calculated_crc = crsf_crc8_update(calculated_crc, my_array[i]);
    }
    my_array[my_array.size() - 1] = calculated_crc;

    CrsfFrameDecoder decoder;
    CrsfFrame frame;

    for (std::size_t i = 0U; i < my_array.size(); ++i)
    {
        ASSERT_EQ(
            decoder.feed_byte(my_array[i], frame),
            i == my_array.size() - 1U);
    }

    EXPECT_EQ(frame.type, 0x16U);
    ASSERT_EQ(frame.payload_length, 60U);

    for (std::size_t i = 0U; i < frame.payload_length; ++i)
    {
        EXPECT_EQ(frame.payload[i], 0x55);
    }

    constexpr std::array<std::uint8_t, 4> second_frame = {
        0xC8, 0x02, 0x00, 0x00};

    for (std::size_t i = 0U; i < second_frame.size(); ++i)
    {
        ASSERT_EQ(
            decoder.feed_byte(second_frame[i], frame),
            i == second_frame.size() - 1U);
    }

    EXPECT_EQ(frame.type, 0x00U);
    EXPECT_EQ(frame.payload_length, 0U);
}