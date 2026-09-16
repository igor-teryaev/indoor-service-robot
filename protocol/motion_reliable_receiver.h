#ifndef ROBOT_PROTOCOL_MOTION_RELIABLE_RECEIVER_H
#define ROBOT_PROTOCOL_MOTION_RELIABLE_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "motion_response_result.h"
#include "motion_transaction.h"

typedef enum
{
    MOTION_RELIABLE_DECISION_INVALID,
    MOTION_RELIABLE_DECISION_NEW,
    MOTION_RELIABLE_DECISION_PENDING_RETRY,
    MOTION_RELIABLE_DECISION_COMPLETED_RETRY,
    MOTION_RELIABLE_DECISION_STALE,
    MOTION_RELIABLE_DECISION_COLLISION
} MotionReliableDecision;

typedef struct
{
    bool latest_sequence_valid;
    uint16_t latest_sequence;

    bool pending_valid;
    MotionTransaction pending_transaction;

    bool terminal_valid;
    MotionTransaction terminal_transaction;
    MotionResponseResult terminal_result;

    uint32_t new_count;
    uint32_t pending_retry_count;
    uint32_t completed_retry_count;
    uint32_t stale_count;
    uint32_t collision_count;
} MotionReliableReceiver;

#ifdef __cplusplus
extern "C" {
#endif

MotionReliableDecision motion_reliable_receiver_classify(
    const MotionReliableReceiver* receiver,
    const MotionTransaction* transaction);

void motion_reliable_receiver_init(
    MotionReliableReceiver* receiver);

void motion_reliable_receiver_reset(
    MotionReliableReceiver* receiver);

void motion_reliable_receiver_begin_pending(
    MotionReliableReceiver* receiver,
    const MotionTransaction* transaction);

void motion_reliable_receiver_record_immediate_terminal(
    MotionReliableReceiver* receiver,
    const MotionTransaction* transaction,
    MotionResponseResult result);

bool motion_reliable_receiver_complete_pending(
    MotionReliableReceiver* receiver,
    const MotionTransaction* transaction,
    MotionResponseResult result);

#ifdef __cplusplus
}
#endif
#endif