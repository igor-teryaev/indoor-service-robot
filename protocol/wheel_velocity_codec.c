#include "wheel_velocity_codec.h"
#include <stdint.h>

void wheel_velocity_command_encode(
    const WheelVelocityCommand* command,
    uint8_t output[WHEEL_VELOCITY_COMMAND_WIRE_SIZE])
{
    const uint16_t left =
        (uint16_t)command->left_velocity_mm_s;

    const uint16_t right =
        (uint16_t)command->right_velocity_mm_s;

    output[0] = (uint8_t)(left >> 8);
    output[1] = (uint8_t)left;

    output[2] = (uint8_t)(right >> 8);
    output[3] = (uint8_t)right;
}

void wheel_velocity_command_decode(
    const uint8_t input[WHEEL_VELOCITY_COMMAND_WIRE_SIZE],
    WheelVelocityCommand* command)
{
    const uint16_t left =
        ((uint16_t)input[0] << 8) |
        (uint16_t)input[1];

    const uint16_t right =
        ((uint16_t)input[2] << 8) |
        (uint16_t)input[3];

    const int32_t left_signed =
        left <= INT16_MAX
            ? (int32_t)left
            : (int32_t)left - 65536L;

    const int32_t right_signed =
        right <= INT16_MAX
            ? (int32_t)right
            : (int32_t)right - 65536L;

    command->left_velocity_mm_s =
        (int16_t)left_signed;

    command->right_velocity_mm_s =
        (int16_t)right_signed;
}