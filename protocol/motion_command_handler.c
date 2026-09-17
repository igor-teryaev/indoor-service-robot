#include "motion_command_handler.h"

#include "motion_lifecycle_command_codec.h"
#include "motion_lifecycle_command_payload.h"

MotionLifecycleAction motion_command_handler_handle(
    MotionLifecycleCoordinator* coordinator,
    const ProtocolFrame* frame)
{
    MotionLifecycleCommandPayload payload;

    motion_lifecycle_command_decode(
        frame->payload,
        &payload
    );

    const MotionTransaction transaction = {
        .sequence = frame->sequence,
        .command = payload.command,
        .motion_session_id = payload.motion_session_id
    };

    return motion_lifecycle_coordinator_handle_transaction(
        coordinator,
        &transaction
    );
}