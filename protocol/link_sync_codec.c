#include "link_sync_codec.h"

#include "byte_codec.h"

void link_sync_encode(
    const LinkSyncPayload* payload,
    uint8_t output[LINK_SYNC_WIRE_SIZE])
{
    byte_codec_encode_u64_be(payload->sync_token, output);
}

void link_sync_decode(
    const uint8_t input[LINK_SYNC_WIRE_SIZE],
    LinkSyncPayload* payload)
{
    payload->sync_token = byte_codec_decode_u64_be(input);
}
