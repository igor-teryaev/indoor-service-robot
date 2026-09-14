#ifndef ROBOT_PROTOCOL_WHEEL_VELOCITY_PAYLOAD_H
#define ROBOT_PROTOCOL_WHEEL_VELOCITY_PAYLOAD_H

#include <stdint.h>

#include "wheel_velocity_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t motion_session_id;
    WheelVelocityCommand command;
} WheelVelocityPayload;

#ifdef __cplusplus
}
#endif

#endif