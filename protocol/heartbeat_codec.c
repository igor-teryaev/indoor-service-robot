#include "heartbeat_codec.h"

#include "byte_codec.h"

void heartbeat_encode(
    const HeartbeatPayload* payload,
    uint8_t output[HEARTBEAT_WIRE_SIZE])
{
    output[0] = payload->link_state;
    byte_codec_encode_u32_be(payload->uptime_ms, output + 1);
}

void heartbeat_decode(
    const uint8_t input[HEARTBEAT_WIRE_SIZE],
    HeartbeatPayload* payload)
{
    payload->link_state = input[0];
    payload->uptime_ms = byte_codec_decode_u32_be(input + 1);
}
