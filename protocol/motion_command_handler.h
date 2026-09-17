#ifndef ROBOT_PROTOCOL_MOTION_COMMAND_HANDLER_H
#define ROBOT_PROTOCOL_MOTION_COMMAND_HANDLER_H

#include "motion_lifecycle_action.h"
#include "motion_lifecycle_coordinator.h"
#include "protocol_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

MotionLifecycleAction motion_command_handler_handle(
    MotionLifecycleCoordinator* coordinator,
    const ProtocolFrame* frame);

#ifdef __cplusplus
}
#endif

#endif