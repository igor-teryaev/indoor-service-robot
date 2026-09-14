#include "byte_codec.h"

uint16_t byte_codec_decode_u16_be(const uint8_t *bytes)
{
    return (uint16_t)(
        ((uint32_t)bytes[0] << 8U) |
        (uint32_t)bytes[1]);
}

int16_t byte_codec_decode_i16_be(const uint8_t *bytes)
{
    const uint16_t value = byte_codec_decode_u16_be(bytes);
    /* Both branches fit int16_t; never cast an out-of-range unsigned value. */
    const int32_t signed_value = value <= INT16_MAX
        ? (int32_t)value
        : (int32_t)value - INT32_C(65536);

    return (int16_t)signed_value;
}

uint32_t byte_codec_decode_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U) |
           ((uint32_t)bytes[1] << 16U) |
           ((uint32_t)bytes[2] << 8U) |
           (uint32_t)bytes[3];
}

uint64_t byte_codec_decode_u64_be(const uint8_t *bytes)
{
    return ((uint64_t)byte_codec_decode_u32_be(bytes) << 32U) |
           (uint64_t)byte_codec_decode_u32_be(bytes + 4);
}

void byte_codec_encode_u16_be(uint16_t value, uint8_t *output)
{
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

void byte_codec_encode_i16_be(int16_t value, uint8_t *output)
{
    /* Signed-to-unsigned conversion is defined modulo 65536, including MIN. */
    byte_codec_encode_u16_be((uint16_t)value, output);
}

void byte_codec_encode_u32_be(uint32_t value, uint8_t *output)
{
    output[0] = (uint8_t)(value >> 24U);
    output[1] = (uint8_t)(value >> 16U);
    output[2] = (uint8_t)(value >> 8U);
    output[3] = (uint8_t)value;
}

void byte_codec_encode_u64_be(uint64_t value, uint8_t *output)
{
    byte_codec_encode_u32_be((uint32_t)(value >> 32U), output);
    byte_codec_encode_u32_be((uint32_t)value, output + 4);
}
