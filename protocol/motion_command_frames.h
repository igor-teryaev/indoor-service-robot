#ifndef ROBOT_PROTOCOL_MOTION_COMMAND_FRAMES_H
#define ROBOT_PROTOCOL_MOTION_COMMAND_FRAMES_H

#include <stdbool.h>

#include "protocol_frame.h"
#include "motion_lifecycle_action.h"

typedef struct
{
    bool ack_valid;
    ProtocolFrame ack_frame;

    bool response_valid;
    ProtocolFrame response_frame;
} MotionCommandFrames;

#ifdef __cplusplus
extern "C" {
#endif

MotionCommandFrames motion_command_frames_build(
    const MotionLifecycleAction* action);

#ifdef __cplusplus
}
#endif

#endif