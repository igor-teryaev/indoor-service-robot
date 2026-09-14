#include "wheel_velocity_payload_codec.h"

#include "byte_codec.h"
#include "wheel_velocity_codec.h"

void wheel_velocity_payload_encode(
    const WheelVelocityPayload* payload,
    uint8_t output[WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE])
{
    byte_codec_encode_u32_be(
        payload->motion_session_id,
        output);

    wheel_velocity_command_encode(
        &payload->command,
        output + 4);
}

void wheel_velocity_payload_decode(
    const uint8_t input[WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE],
    WheelVelocityPayload* payload)
{
    payload->motion_session_id =
        byte_codec_decode_u32_be(input);

    wheel_velocity_command_decode(
        input + 4,
        &payload->command);
}