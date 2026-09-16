#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "crc16_ccitt_false.h"

TEST(Crc16CcittFalseTest, MatchesStandardCheckVector)
{
    const std::array<uint8_t, 9U> data{
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };

    const uint16_t crc =
        crc16_ccitt_false(data.data(), data.size());

    EXPECT_EQ(crc, 0x29B1U);
}

TEST(Crc16CcittFalseTest, EmptyInputReturnsInitialValue)
{
    const uint16_t crc =
        crc16_ccitt_false(nullptr, 0U);

    EXPECT_EQ(crc, 0xFFFFU);
}

TEST(Crc16CcittFalseTest, IncrementalUpdateMatchesSinglePassCalculation)
{
    static const std::array<uint8_t, 4U> first_part{
        '1', '2', '3', '4'
    };

    static const std::array<uint8_t, 5U> second_part{
        '5', '6', '7', '8', '9'
    };

    uint16_t crc = CRC16_CCITT_FALSE_INITIAL;

    crc = crc16_ccitt_false_update(
        crc,
        first_part.data(),
        first_part.size()
    );

    crc = crc16_ccitt_false_update(
        crc,
        second_part.data(),
        second_part.size()
    );

    EXPECT_EQ(crc, 0x29B1U);
}