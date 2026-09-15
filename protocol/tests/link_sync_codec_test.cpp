#include <gtest/gtest.h>
#include <array>

#include "link_sync_codec.h"

TEST(LinkSyncCodecTest, EncodesLinkSyncPayload)
{
    const LinkSyncPayload payload{
        .sync_token = UINT64_C(0x0123456789ABCDEF)
    };

    std::array<uint8_t, LINK_SYNC_WIRE_SIZE> output{};
    link_sync_encode(&payload, output.data());

    const std::array<uint8_t, LINK_SYNC_WIRE_SIZE> expected{
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
    };
    EXPECT_EQ(output, expected);
}

TEST(LinkSyncCodecTest, DecodesLinkSyncPayload)
{

    const std::array<uint8_t, LINK_SYNC_WIRE_SIZE> input{
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
    };
    LinkSyncPayload payload{};

    link_sync_decode(input.data(), &payload);

    EXPECT_EQ(payload.sync_token, UINT64_C(0x0123456789ABCDEF));
}