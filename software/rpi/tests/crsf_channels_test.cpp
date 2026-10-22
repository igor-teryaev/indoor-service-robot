#include <gtest/gtest.h>
#include <array>
#include "crsf_channels.h"

TEST(CrsfChannelsTest, DecodesCapturedChannels)
{
    CrsfFrame frame{
        .type = 0x16U,
        .payload = {
            0xE2, 0xDB, 0x1E, 0xF8, 0xC0, 0xF7,
            0x8B, 0xF2, 0x95, 0xAF, 0x7C, 0xE5,
            0x2B, 0x5F, 0xF9, 0xCA, 0x07, 0x00,
            0x00, 0x4C, 0x7C, 0xE2
        },
        .payload_length = 22U
    };

    CrsfChannels channels{};
    ASSERT_TRUE(crsf_channels_decode(frame, channels));

    EXPECT_EQ(channels.values[0], 994U);
    EXPECT_EQ(channels.values[1], 987U);
    EXPECT_EQ(channels.values[4], 191U);
}

TEST(CrsfChannelsTest, RejectsInvalidFramesWithoutChangingOutput)
{
    CrsfChannels channels{};
    channels.values.fill(1234U);
    const auto original = channels.values;

    CrsfFrame frame{
        .type = 0x14U,
        .payload = {
            0xE2, 0xDB, 0x1E, 0xF8, 0xC0, 0xF7,
            0x8B, 0xF2, 0x95, 0xAF, 0x7C, 0xE5,
            0x2B, 0x5F, 0xF9, 0xCA, 0x07, 0x00,
            0x00, 0x4C, 0x7C, 0xE2
        },
        .payload_length = 22U
    };
    EXPECT_FALSE(crsf_channels_decode(frame, channels));
    EXPECT_EQ(channels.values, original);

    frame.type = 0x16U;
    frame.payload_length = 21U;
    EXPECT_FALSE(crsf_channels_decode(frame, channels));
    EXPECT_EQ(channels.values, original);

    frame.type = 0x16U;
    frame.payload_length = 23U;
    EXPECT_FALSE(crsf_channels_decode(frame, channels));
    EXPECT_EQ(channels.values, original);
}