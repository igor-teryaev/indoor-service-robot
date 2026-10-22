#include <gtest/gtest.h>
#include <cstddef>
#include "crsf_crc8.h"

TEST(CrsfCrc8Test, MatchesCapturedLinkStatisticsFrame)
{
    constexpr std::uint8_t bytes[] = {
        0x14, 0x1B, 0x17, 0x00, 0x0C, 0x01,
        0x07, 0x03, 0x00, 0x00, 0x00
    };

    std::uint8_t crc = 0U;
    for (const auto byte : bytes)
    {
        crc = crsf_crc8_update(crc, byte);
    }

    EXPECT_EQ(crc, 0x95U);
}

TEST(CrsfCrc8Test, MatchesStandardCheckVector)
{
    constexpr char bytes[] = "123456789";

    std::uint8_t crc = 0U;
    for (std::size_t i = 0U; i < sizeof(bytes) - 1U; ++i)
    {
        crc = crsf_crc8_update(
            crc,
            static_cast<std::uint8_t>(bytes[i]));
    }

    EXPECT_EQ(crc, 0xBCU);
}