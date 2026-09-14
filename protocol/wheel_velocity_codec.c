#include "wheel_velocity_codec.h"

#include "byte_codec.h"

void wheel_velocity_command_encode(
    const WheelVelocityCommand* command,
    uint8_t output[WHEEL_VELOCITY_COMMAND_WIRE_SIZE])
{
    byte_codec_encode_i16_be(command->left_velocity_mm_s, output);
    byte_codec_encode_i16_be(command->right_velocity_mm_s, output + 2);
}

void wheel_velocity_command_decode(
    const uint8_t input[WHEEL_VELOCITY_COMMAND_WIRE_SIZE],
    WheelVelocityCommand* command)
{
    command->left_velocity_mm_s = byte_codec_decode_i16_be(input);
    command->right_velocity_mm_s = byte_codec_decode_i16_be(input + 2);
}
