#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "byte_codec.h"

namespace {

template <typename T, std::size_t N>
struct WireCase {
    T value;
    std::array<uint8_t, N> bytes;
};

template <typename T, std::size_t N, std::size_t Count>
void check_vectors(
    const WireCase<T, N> (&cases)[Count],
    void (*encode)(T, uint8_t*),
    T (*decode)(const uint8_t*))
{
    for (const auto& item : cases) {
        SCOPED_TRACE(::testing::Message() << "value=" << item.value);
        // Offset one exercises byte access without integer alignment.
        alignas(uint64_t) std::array<uint8_t, N + 2> output;
        output.fill(0xA5);
        encode(item.value, output.data() + 1);
        EXPECT_EQ(output.front(), 0xA5);
        EXPECT_EQ(output.back(), 0xA5);
        for (std::size_t i = 0; i < N; ++i) {
            EXPECT_EQ(output[i + 1], item.bytes[i]) << "byte=" << i;
        }

        // Decode the literal vector, independently of encode's output.
        alignas(uint64_t) std::array<uint8_t, N + 1> input{};
        for (std::size_t i = 0; i < N; ++i) {
            input[i + 1] = item.bytes[i];
        }
        const auto original = input;
        EXPECT_EQ(decode(input.data() + 1), item.value);
        EXPECT_EQ(input, original);
    }
}

} // namespace

TEST(ByteCodecTest, U16KnownWireVectors)
{
    const WireCase<uint16_t, 2> cases[]{
        {0, {0x00, 0x00}},
        {1, {0x00, 0x01}},
        {0x1234, {0x12, 0x34}},
        {0x8000, {0x80, 0x00}},
        {UINT16_MAX, {0xFF, 0xFF}},
    };
    check_vectors(cases, byte_codec_encode_u16_be, byte_codec_decode_u16_be);
}

TEST(ByteCodecTest, I16KnownWireVectors)
{
    const WireCase<int16_t, 2> cases[]{
        {0, {0x00, 0x00}},
        {1, {0x00, 0x01}},
        {-1, {0xFF, 0xFF}},
        {300, {0x01, 0x2C}},
        {-125, {0xFF, 0x83}},
        {INT16_MIN, {0x80, 0x00}},
        {INT16_MAX, {0x7F, 0xFF}},
    };
    check_vectors(cases, byte_codec_encode_i16_be, byte_codec_decode_i16_be);
}

TEST(ByteCodecTest, U32KnownWireVectors)
{
    const WireCase<uint32_t, 4> cases[]{
        {0, {0x00, 0x00, 0x00, 0x00}},
        {1, {0x00, 0x00, 0x00, 0x01}},
        {UINT32_C(0x12345678), {0x12, 0x34, 0x56, 0x78}},
        {UINT32_C(0x80000000), {0x80, 0x00, 0x00, 0x00}},
        {UINT32_MAX, {0xFF, 0xFF, 0xFF, 0xFF}},
    };
    check_vectors(cases, byte_codec_encode_u32_be, byte_codec_decode_u32_be);
}

TEST(ByteCodecTest, U64KnownWireVectors)
{
    const WireCase<uint64_t, 8> cases[]{
        {0, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {1, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}},
        {UINT64_C(0x0123456789ABCDEF),
            {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF}},
        {UINT64_C(0x00000000FFFFFFFF),
            {0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF}},
        {UINT64_C(0x0000000100000000),
            {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}},
        {UINT64_C(0x8000000000000000),
            {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {UINT64_MAX, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    };
    check_vectors(cases, byte_codec_encode_u64_be, byte_codec_decode_u64_be);
}

TEST(ByteCodecTest, EncodesEntireI16Range)
{
    for (int32_t value = INT16_MIN; value <= INT16_MAX; ++value) {
        // Arithmetic oracle, separate from the implementation's unsigned cast.
        const int32_t wire = value < 0 ? value + 65536 : value;
        uint8_t output[2]{};
        byte_codec_encode_i16_be(static_cast<int16_t>(value), output);
        ASSERT_EQ(output[0], wire / 256) << "value=" << value;
        ASSERT_EQ(output[1], wire % 256) << "value=" << value;
    }
}

TEST(ByteCodecTest, DecodesEveryI16BitPattern)
{
    for (int32_t high = 0; high < 256; ++high) {
        for (int32_t low = 0; low < 256; ++low) {
            const uint8_t input[]{
                static_cast<uint8_t>(high), static_cast<uint8_t>(low)};
            // Interpret the high byte as signed, then add the low byte.
            const int32_t expected = (high < 128 ? high : high - 256) * 256 + low;
            ASSERT_EQ(byte_codec_decode_i16_be(input), expected)
                << "high=" << high << " low=" << low;
        }
    }
}
