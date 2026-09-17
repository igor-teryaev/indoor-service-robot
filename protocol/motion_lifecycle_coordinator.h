#ifndef ROBOT_PROTOCOL_MOTION_LIFECYCLE_COORDINATOR_H
#define ROBOT_PROTOCOL_MOTION_LIFECYCLE_COORDINATOR_H

#include <stdint.h>

#include "motion_lifecycle_state.h"
#include "motion_reliable_receiver.h"
#include "motion_lifecycle_action.h"
#include "motion_transaction.h"
#include "motion_lifecycle_operation_result.h"

typedef struct
{
    MotionLifecycleState state;

    uint32_t motion_session_id;

    MotionReliableReceiver reliable_receiver;

    uint32_t next_operation_id;
    uint32_t active_operation_id;

} MotionLifecycleCoordinator;

#ifdef __cplusplus
extern "C" {
#endif

void motion_lifecycle_coordinator_init(
    MotionLifecycleCoordinator* coordinator);

void motion_lifecycle_coordinator_reset(
    MotionLifecycleCoordinator* coordinator);

MotionLifecycleAction motion_lifecycle_coordinator_handle_transaction(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction);

MotionLifecycleAction motion_lifecycle_coordinator_complete_operation(
    MotionLifecycleCoordinator* coordinator,
    uint32_t operation_id,
    MotionLifecycleOperationResult result);

#ifdef __cplusplus
}
#endif

#endif