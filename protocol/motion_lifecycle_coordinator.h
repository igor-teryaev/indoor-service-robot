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
/*
 * Initializes a fresh coordinator instance.
 *
 * Must only be called when no asynchronous operation completions from a
 * previous lifetime of this object can still arrive.
 *
 * For runtime protocol/session reset, use
 * motion_lifecycle_coordinator_reset() instead.
 */
void motion_lifecycle_coordinator_init(
    MotionLifecycleCoordinator* coordinator);
/*
 * Invalidates the current lifecycle and active physical operation while
 * preserving the operation ID generation counter.
 *
 * Late completions from operations started before reset are therefore
 * rejected.
 */
void motion_lifecycle_coordinator_reset(
    MotionLifecycleCoordinator* coordinator);

MotionLifecycleAction motion_lifecycle_coordinator_handle_transaction(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction);
/*
 * Completes the currently active physical operation.
 *
 * operation_id must match the non-zero ID returned with the corresponding
 * ENSURE_STOPPED action. Zero, stale, wrong, or duplicate IDs are ignored.
 *
 * Operation IDs may repeat only after a full uint32_t non-zero ID cycle.
 */
MotionLifecycleAction motion_lifecycle_coordinator_complete_operation(
    MotionLifecycleCoordinator* coordinator,
    uint32_t operation_id,
    MotionLifecycleOperationResult result);

#ifdef __cplusplus
}
#endif

#endif