#include "motion_lifecycle_command_codec.h"

#include "byte_codec.h"

void motion_lifecycle_command_encode(
    const MotionLifecycleCommandPayload* payload,
    uint8_t output[MOTION_LIFECYCLE_COMMAND_WIRE_SIZE])
{
    output[0] = payload->command;

    byte_codec_encode_u32_be(
        payload->motion_session_id,
        output + 1);
}

void motion_lifecycle_command_decode(
    const uint8_t input[MOTION_LIFECYCLE_COMMAND_WIRE_SIZE],
    MotionLifecycleCommandPayload* payload)
{
    payload->command = input[0];

    payload->motion_session_id =
        byte_codec_decode_u32_be(input + 1);
}