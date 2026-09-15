#ifndef ROBOT_PROTOCOL_MOTION_LIFECYCLE_COMMAND_CODEC_H
#define ROBOT_PROTOCOL_MOTION_LIFECYCLE_COMMAND_CODEC_H

#include <stdint.h>

#include "motion_lifecycle_command_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_LIFECYCLE_COMMAND_WIRE_SIZE 5U

void motion_lifecycle_command_encode(
    const MotionLifecycleCommandPayload* payload,
    uint8_t output[MOTION_LIFECYCLE_COMMAND_WIRE_SIZE]);

void motion_lifecycle_command_decode(
    const uint8_t input[MOTION_LIFECYCLE_COMMAND_WIRE_SIZE],
    MotionLifecycleCommandPayload* payload);

#ifdef __cplusplus
}
#endif

#endif