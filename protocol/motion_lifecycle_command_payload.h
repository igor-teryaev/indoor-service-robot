#ifndef ROBOT_PROTOCOL_MOTION_LIFECYCLE_COMMAND_PAYLOAD_H
#define ROBOT_PROTOCOL_MOTION_LIFECYCLE_COMMAND_PAYLOAD_H

#include <stdint.h>

#include "motion_lifecycle_command_type.h"

typedef struct
{
    MotionLifecycleCommandType command;
    uint32_t motion_session_id;
} MotionLifecycleCommandPayload;

#endif
