#ifndef ROBOT_PROTOCOL_BYTE_CODEC_H
#define ROBOT_PROTOCOL_BYTE_CODEC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed-width big-endian byte primitives, independent of host endianness.
 * The caller must supply a valid non-null buffer of at least 2 bytes for
 * u16/i16, 4 bytes for u32, or 8 bytes for u64. Exactly that many bytes are
 * read or written; no alignment is required. Length/null checks belong to
 * the enclosing payload/frame codec. No allocation or protocol state.
 * Signed i16 uses a two's-complement wire representation.
 */
uint16_t byte_codec_decode_u16_be(const uint8_t *bytes);
int16_t byte_codec_decode_i16_be(const uint8_t *bytes);
uint32_t byte_codec_decode_u32_be(const uint8_t *bytes);
uint64_t byte_codec_decode_u64_be(const uint8_t *bytes);

void byte_codec_encode_u16_be(uint16_t value, uint8_t *output);
void byte_codec_encode_i16_be(int16_t value, uint8_t *output);
void byte_codec_encode_u32_be(uint32_t value, uint8_t *output);
void byte_codec_encode_u64_be(uint64_t value, uint8_t *output);

#ifdef __cplusplus
}
#endif

#endif
