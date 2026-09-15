#include "motion_response_codec.h"
#include "byte_codec.h"

void motion_response_encode(
    const MotionResponsePayload* payload,
    uint8_t output[MOTION_RESPONSE_WIRE_SIZE])
{
    output[0] = payload->command;
    byte_codec_encode_u32_be(payload->motion_session_id, output + 1);
    output[5] = payload->result;
}

void motion_response_decode(
    const uint8_t input[MOTION_RESPONSE_WIRE_SIZE],
    MotionResponsePayload* payload)
{
    payload->command = input[0];
    payload->motion_session_id = byte_codec_decode_u32_be(input + 1);
    payload->result = input[5];
}