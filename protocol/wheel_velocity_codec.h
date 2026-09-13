#pragma once

#include <stdint.h>

#include "wheel_velocity_command.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WHEEL_VELOCITY_COMMAND_WIRE_SIZE 4U
/*
 * Wheel velocity wire format:
 *
 *   byte 0: left velocity, bits 15..8
 *   byte 1: left velocity, bits  7..0
 *   byte 2: right velocity, bits 15..8
 *   byte 3: right velocity, bits  7..0
 *
 * Each velocity is a signed 16-bit two's-complement value
 * expressed in millimeters per second and encoded in big-endian order.
 *
 * The caller must provide valid non-null pointers.
 * The input/output buffer must contain at least
 * WHEEL_VELOCITY_COMMAND_WIRE_SIZE bytes.
 */
void wheel_velocity_command_encode(
    const WheelVelocityCommand* command,
    uint8_t output[WHEEL_VELOCITY_COMMAND_WIRE_SIZE]);

void wheel_velocity_command_decode(
    const uint8_t input[WHEEL_VELOCITY_COMMAND_WIRE_SIZE],
    WheelVelocityCommand* command);

#ifdef __cplusplus
}
#endif