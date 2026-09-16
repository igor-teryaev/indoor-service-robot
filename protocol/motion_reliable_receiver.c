#include <stddef.h>

#include "motion_reliable_receiver.h"

#include <string.h>

#include "motion_sequence.h"

MotionReliableDecision motion_reliable_receiver_classify(
    const MotionReliableReceiver* receiver,
    const MotionTransaction* transaction)
{
    if ((receiver == NULL) || (transaction == NULL))
    {
        return MOTION_RELIABLE_DECISION_INVALID;
    }

    if (!receiver->latest_sequence_valid)
    {
        return MOTION_RELIABLE_DECISION_NEW;
    }

    const MotionSequenceRelation relation =
        motion_sequence_classify(
            receiver->latest_sequence,
            transaction->sequence
        );

    switch (relation)
    {
    case MOTION_SEQUENCE_NEWER:
        return MOTION_RELIABLE_DECISION_NEW;

    case MOTION_SEQUENCE_STALE:
        return MOTION_RELIABLE_DECISION_STALE;

    case MOTION_SEQUENCE_SAME:
        break;

    default:
        return MOTION_RELIABLE_DECISION_INVALID;
    }

    if (receiver->terminal_valid &&
        (receiver->terminal_transaction.sequence ==
         receiver->latest_sequence))
    {
        if (motion_transaction_equal(
                &receiver->terminal_transaction,
                transaction))
        {
            return MOTION_RELIABLE_DECISION_COMPLETED_RETRY;
        }

        return MOTION_RELIABLE_DECISION_COLLISION;
    }

    if (receiver->pending_valid &&
        (receiver->pending_transaction.sequence ==
         receiver->latest_sequence))
    {
        if (motion_transaction_equal(
                &receiver->pending_transaction,
                transaction))
        {
            return MOTION_RELIABLE_DECISION_PENDING_RETRY;
        }

        return MOTION_RELIABLE_DECISION_COLLISION;
    }

    return MOTION_RELIABLE_DECISION_INVALID;
}

void motion_reliable_receiver_init(
    MotionReliableReceiver* receiver)
{
    memset(receiver, 0, sizeof(*receiver));
}

void motion_reliable_receiver_reset(
    MotionReliableReceiver* receiver)
{
    receiver->latest_sequence_valid = false;
    receiver->latest_sequence = 0U;

    receiver->pending_valid = false;
    receiver->pending_transaction = (MotionTransaction){0};

    receiver->terminal_valid = false;
    receiver->terminal_transaction = (MotionTransaction){0};
    receiver->terminal_result = 0U;
}

void motion_reliable_receiver_begin_pending(
    MotionReliableReceiver* receiver,
    const MotionTransaction* transaction)
{
    const MotionTransaction new_transaction = *transaction;

    receiver->terminal_valid = false;
    receiver->terminal_transaction = (MotionTransaction){0};
    receiver->terminal_result = 0U;

    receiver->pending_transaction = new_transaction;
    receiver->latest_sequence = new_transaction.sequence;

    receiver->latest_sequence_valid = true;
    receiver->pending_valid = true;
}

void motion_reliable_receiver_record_immediate_terminal(
    MotionReliableReceiver* receiver,
    const MotionTransaction* transaction,
    MotionResponseResult result)
{
    const MotionTransaction new_transaction = *transaction;
    receiver->latest_sequence = new_transaction.sequence;
    receiver->terminal_transaction = new_transaction;
    receiver->terminal_result = result;

    receiver->terminal_valid = true;
    receiver->latest_sequence_valid = true;
}

bool motion_reliable_receiver_complete_pending(
    MotionReliableReceiver* receiver,
    const MotionTransaction* transaction,
    MotionResponseResult result)
{
    if ((!receiver->pending_valid) ||
        (!motion_transaction_equal(&receiver->pending_transaction, transaction)))
    {
        return false;
    }

    const MotionTransaction pending_transaction = receiver->pending_transaction;

    receiver->pending_valid = false;
    receiver->pending_transaction = (MotionTransaction){0};

    if (receiver->latest_sequence_valid &&
        (pending_transaction.sequence == receiver->latest_sequence))
    {
        receiver->terminal_transaction = pending_transaction;
        receiver->terminal_result = result;
        receiver->terminal_valid = true;
    }
    return true;
}