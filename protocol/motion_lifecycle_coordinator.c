#include "motion_lifecycle_coordinator.h"
#include "motion_lifecycle_action.h"

static uint32_t begin_new_operation(
    MotionLifecycleCoordinator* coordinator)
{
    coordinator->next_operation_id++;

    if (coordinator->next_operation_id == 0U)
    {
        coordinator->next_operation_id++;
    }

    coordinator->active_operation_id =
        coordinator->next_operation_id;

    return coordinator->active_operation_id;
}

void motion_lifecycle_coordinator_init(MotionLifecycleCoordinator* coordinator)
{
    coordinator->state = MOTION_LIFECYCLE_STATE_NO_SESSION;
    coordinator->motion_session_id = 0U;
    coordinator->next_operation_id = 0U;
    coordinator->active_operation_id = 0U;

    motion_reliable_receiver_init(&coordinator->reliable_receiver);
}

void motion_lifecycle_coordinator_reset(MotionLifecycleCoordinator* coordinator)
{
    coordinator->state = MOTION_LIFECYCLE_STATE_NO_SESSION;

    coordinator->motion_session_id = 0U;
    coordinator->active_operation_id = 0U;

    motion_reliable_receiver_reset(&coordinator->reliable_receiver);
}

static MotionLifecycleAction handle_start_no_session(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    motion_reliable_receiver_begin_pending(
        &coordinator->reliable_receiver,
        transaction
    );

    coordinator->state =
        MOTION_LIFECYCLE_STATE_STARTING;

    coordinator->motion_session_id =
        transaction->motion_session_id;

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.operation = MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED;

    action.operation_id = begin_new_operation(coordinator);

    return action;
}

static MotionLifecycleAction handle_start_already_active(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    motion_reliable_receiver_record_immediate_terminal(
        &coordinator->reliable_receiver,
        transaction,
        MOTION_RESPONSE_ALREADY_ACTIVE);

    action.ack_transaction = *transaction;
    action.response_transaction = *transaction;
    action.response_result = MOTION_RESPONSE_ALREADY_ACTIVE;

    action.send_ack = true;
    action.send_response = true;
    return action;
}

static MotionLifecycleAction handle_start_replacing_active(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    motion_reliable_receiver_begin_pending(
        &coordinator->reliable_receiver,
        transaction
    );

    coordinator->state =
        MOTION_LIFECYCLE_STATE_STARTING;

    coordinator->motion_session_id =
        transaction->motion_session_id;

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.operation = MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED;
    action.operation_id = begin_new_operation(coordinator);

    return action;
}

static MotionLifecycleAction handle_end_active(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    motion_reliable_receiver_begin_pending(
        &coordinator->reliable_receiver,
        transaction
    );

    coordinator->state = MOTION_LIFECYCLE_STATE_ENDING;

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.operation = MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED;
    action.operation_id = begin_new_operation(coordinator);

    return action;
}

static MotionLifecycleAction handle_end_session_mismatch(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    motion_reliable_receiver_record_immediate_terminal(
        &coordinator->reliable_receiver,
        transaction,
        MOTION_RESPONSE_SESSION_MISMATCH
    );

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction = *transaction;
    action.response_result = MOTION_RESPONSE_SESSION_MISMATCH;

    return action;
}

static MotionLifecycleAction handle_end_no_session(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    motion_reliable_receiver_record_immediate_terminal(
        &coordinator->reliable_receiver,
        transaction,
        MOTION_RESPONSE_ALREADY_ENDED
    );

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction = *transaction;
    action.response_result = MOTION_RESPONSE_ALREADY_ENDED;

    return action;
}

static MotionLifecycleAction handle_start_while_starting(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    const MotionTransaction superseded_transaction = coordinator->reliable_receiver.pending_transaction;

    motion_reliable_receiver_begin_pending(
        &coordinator->reliable_receiver,
        transaction
    );

    coordinator->motion_session_id = transaction->motion_session_id;

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction = superseded_transaction;

    action.response_result = MOTION_RESPONSE_SUPERSEDED;

    return action;
}

static MotionLifecycleAction handle_end_while_starting(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action =
        motion_lifecycle_action_none();

    const MotionTransaction superseded_transaction = coordinator->reliable_receiver.pending_transaction;

    motion_reliable_receiver_begin_pending(
        &coordinator->reliable_receiver,
        transaction
    );

    coordinator->state = MOTION_LIFECYCLE_STATE_ENDING;

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction =
        superseded_transaction;

    action.response_result = MOTION_RESPONSE_SUPERSEDED;

    return action;
}

static MotionLifecycleAction handle_end_session_mismatch_while_starting(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action =  motion_lifecycle_action_none();

    motion_reliable_receiver_record_immediate_terminal(
        &coordinator->reliable_receiver,
        transaction,
        MOTION_RESPONSE_SESSION_MISMATCH
    );

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction = *transaction;
    action.response_result = MOTION_RESPONSE_SESSION_MISMATCH;

    return action;
}

static MotionLifecycleAction handle_start_while_ending(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    motion_reliable_receiver_record_immediate_terminal(
        &coordinator->reliable_receiver,
        transaction,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction = *transaction;
    action.response_result = MOTION_RESPONSE_BUSY_STOPPING;

    return action;
}

static MotionLifecycleAction handle_end_while_ending(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action =
        motion_lifecycle_action_none();

    const MotionTransaction superseded_transaction = coordinator->reliable_receiver.pending_transaction;

    motion_reliable_receiver_begin_pending(
        &coordinator->reliable_receiver,
        transaction
    );

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction =
        superseded_transaction;

    action.response_result = MOTION_RESPONSE_SUPERSEDED;

    return action;
}

static MotionLifecycleAction handle_end_session_mismatch_while_ending(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action =  motion_lifecycle_action_none();

    motion_reliable_receiver_record_immediate_terminal(
        &coordinator->reliable_receiver,
        transaction,
        MOTION_RESPONSE_SESSION_MISMATCH
    );

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction = *transaction;
    action.response_result = MOTION_RESPONSE_SESSION_MISMATCH;

    return action;
}

static MotionLifecycleAction handle_invalid_command(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    motion_reliable_receiver_record_immediate_terminal(
        &coordinator->reliable_receiver,
        transaction,
        MOTION_RESPONSE_INVALID_COMMAND
    );

    action.send_ack = true;
    action.ack_transaction = *transaction;

    action.send_response = true;
    action.response_transaction = *transaction;
    action.response_result = MOTION_RESPONSE_INVALID_COMMAND;

    return action;
}

MotionLifecycleAction motion_lifecycle_coordinator_complete_operation(
    MotionLifecycleCoordinator* coordinator,
    uint32_t operation_id,
    MotionLifecycleOperationResult result)
{
    MotionLifecycleAction action = motion_lifecycle_action_none();

    if ((operation_id == 0U) || (operation_id != coordinator->active_operation_id))
    {
        return action;
    }

    if (!coordinator->reliable_receiver.pending_valid)
    {
        return action;
    }

    if ((coordinator->state != MOTION_LIFECYCLE_STATE_STARTING) &&
        (coordinator->state != MOTION_LIFECYCLE_STATE_ENDING))
    {
        return action;
    }

    coordinator->active_operation_id = 0U;

    const MotionTransaction transaction = coordinator->reliable_receiver.pending_transaction;

    if (coordinator->state == MOTION_LIFECYCLE_STATE_ENDING)
    {
        const MotionResponseResult response_result =
            (result == MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS)
                ? MOTION_RESPONSE_OK
                : MOTION_RESPONSE_STOP_FAILED;

        coordinator->state = MOTION_LIFECYCLE_STATE_NO_SESSION;

        coordinator->motion_session_id = 0U;

        motion_reliable_receiver_complete_pending(
            &coordinator->reliable_receiver,
            &transaction,
            response_result
        );

        action.send_response = true;
        action.response_transaction = transaction;
        action.response_result = response_result;

        return action;
    }

    MotionResponseResult response_result;

    if (result == MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS)
    {
        coordinator->state = MOTION_LIFECYCLE_STATE_ACTIVE;

        response_result = MOTION_RESPONSE_OK;
    }
    else
    {
        coordinator->state = MOTION_LIFECYCLE_STATE_NO_SESSION;

        coordinator->motion_session_id = 0U;

        response_result = MOTION_RESPONSE_STOP_FAILED;
    }

    motion_reliable_receiver_complete_pending(
        &coordinator->reliable_receiver,
        &transaction,
        response_result
    );

    action.send_response = true;
    action.response_transaction = transaction;
    action.response_result = response_result;

    return action;
}

MotionLifecycleAction motion_lifecycle_coordinator_handle_transaction(
    MotionLifecycleCoordinator* coordinator,
    const MotionTransaction* transaction)
{
    MotionLifecycleAction action =
        motion_lifecycle_action_none();

    const MotionReliableDecision decision =
        motion_reliable_receiver_classify(
            &coordinator->reliable_receiver,
            transaction
        );

    switch (decision)
    {
    case MOTION_RELIABLE_DECISION_PENDING_RETRY:
        action.send_ack = true;
        action.ack_transaction = *transaction;
        return action;

    case MOTION_RELIABLE_DECISION_NEW:
        if ((coordinator->state == MOTION_LIFECYCLE_STATE_ACTIVE) &&
            (transaction->command == MOTION_LIFECYCLE_COMMAND_START_SESSION) &&
            (transaction->motion_session_id == coordinator->motion_session_id))
        {
            return handle_start_already_active(
                coordinator,
                transaction
            );
        }

        if ((coordinator->state ==  MOTION_LIFECYCLE_STATE_ACTIVE) &&
            (transaction->command == MOTION_LIFECYCLE_COMMAND_START_SESSION))
        {
            return handle_start_replacing_active(
                coordinator,
                transaction
            );
        }

        if ((coordinator->state == MOTION_LIFECYCLE_STATE_ACTIVE) &&
            (transaction->command == MOTION_LIFECYCLE_COMMAND_END_SESSION) &&
            (transaction->motion_session_id == coordinator->motion_session_id))
        {
            return handle_end_active(
                coordinator,
                transaction
            );
        }

        if ((coordinator->state == MOTION_LIFECYCLE_STATE_ACTIVE) &&
            (transaction->command == MOTION_LIFECYCLE_COMMAND_END_SESSION))
        {
            return handle_end_session_mismatch(
                coordinator,
                transaction
            );
        }

        break;

    case MOTION_RELIABLE_DECISION_COMPLETED_RETRY:
        action.send_ack = true;
        action.ack_transaction = coordinator->reliable_receiver.terminal_transaction;

        action.send_response = true;
        action.response_transaction = coordinator->reliable_receiver.terminal_transaction;

        action.response_result = coordinator->reliable_receiver.terminal_result;

        return action;

    case MOTION_RELIABLE_DECISION_INVALID:
    case MOTION_RELIABLE_DECISION_STALE:
    case MOTION_RELIABLE_DECISION_COLLISION:
    default:
        return action;
    }

    if ((coordinator->state == MOTION_LIFECYCLE_STATE_STARTING) &&
    (transaction->command == MOTION_LIFECYCLE_COMMAND_START_SESSION))
    {
        return handle_start_while_starting(
            coordinator,
            transaction
        );
    }

    if ((coordinator->state == MOTION_LIFECYCLE_STATE_NO_SESSION) &&
        (transaction->command == MOTION_LIFECYCLE_COMMAND_START_SESSION))
    {
        return handle_start_no_session(
            coordinator,
            transaction
        );
    }

    if ((coordinator->state == MOTION_LIFECYCLE_STATE_NO_SESSION) &&
        (transaction->command == MOTION_LIFECYCLE_COMMAND_END_SESSION))
    {
        return handle_end_no_session(
            coordinator,
            transaction
        );
    }

    if ((coordinator->state == MOTION_LIFECYCLE_STATE_STARTING) &&
        (transaction->command == MOTION_LIFECYCLE_COMMAND_END_SESSION) &&
        (transaction->motion_session_id == coordinator->motion_session_id))
    {
        return handle_end_while_starting(
            coordinator,
            transaction
        );
    }

    if ((coordinator->state == MOTION_LIFECYCLE_STATE_STARTING) &&
    (transaction->command == MOTION_LIFECYCLE_COMMAND_END_SESSION))
    {
        return handle_end_session_mismatch_while_starting(
            coordinator,
            transaction
        );
    }

    if ((coordinator->state == MOTION_LIFECYCLE_STATE_ENDING) &&
    (transaction->command == MOTION_LIFECYCLE_COMMAND_START_SESSION))
    {
        return handle_start_while_ending(
            coordinator,
            transaction
        );
    }

    if ((coordinator->state == MOTION_LIFECYCLE_STATE_ENDING) &&
    (transaction->command == MOTION_LIFECYCLE_COMMAND_END_SESSION) &&
    (transaction->motion_session_id == coordinator->motion_session_id))
    {
        return handle_end_while_ending(
            coordinator,
            transaction
        );
    }

    if ((coordinator->state == MOTION_LIFECYCLE_STATE_ENDING) &&
    (transaction->command == MOTION_LIFECYCLE_COMMAND_END_SESSION))
    {
        return handle_end_session_mismatch_while_ending(
            coordinator,
            transaction
        );
    }

    if ((transaction->command != MOTION_LIFECYCLE_COMMAND_START_SESSION) &&
    (transaction->command != MOTION_LIFECYCLE_COMMAND_END_SESSION))
    {
        return handle_invalid_command(
            coordinator,
            transaction
        );
    }

    return action;
}