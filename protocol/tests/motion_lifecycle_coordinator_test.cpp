#include <gtest/gtest.h>

#include "motion_lifecycle_coordinator.h"

namespace {
    MotionLifecycleAction complete_active_operation(
    MotionLifecycleCoordinator* coordinator,
    MotionLifecycleOperationResult result)
    {
        return motion_lifecycle_coordinator_complete_operation(
            coordinator,
            coordinator->active_operation_id,
            result
        );
    }
}

TEST(MotionLifecycleCoordinatorTest, InitClearsLifecycleAndReceiverState)
{
    MotionLifecycleCoordinator coordinator{};

    coordinator.state = MOTION_LIFECYCLE_STATE_ACTIVE;
    coordinator.motion_session_id = 107U;

    coordinator.reliable_receiver.new_count = 3U;
    coordinator.reliable_receiver.stale_count = 4U;

    motion_lifecycle_coordinator_init(&coordinator);

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(coordinator.motion_session_id, 0U);

    EXPECT_FALSE(
        coordinator.reliable_receiver.latest_sequence_valid
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.new_count,
        0U
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.stale_count,
        0U
    );
}

TEST(MotionLifecycleCoordinatorTest, ResetClearsLifecycleButPreservesReceiverDiagnostics)
{
    MotionLifecycleCoordinator coordinator{};

    coordinator.state = MOTION_LIFECYCLE_STATE_ACTIVE;
    coordinator.motion_session_id = 107U;

    coordinator.reliable_receiver.latest_sequence_valid = true;
    coordinator.reliable_receiver.latest_sequence = 42U;

    coordinator.reliable_receiver.new_count = 3U;
    coordinator.reliable_receiver.stale_count = 4U;

    motion_lifecycle_coordinator_reset(&coordinator);

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(coordinator.motion_session_id, 0U);

    EXPECT_FALSE(
        coordinator.reliable_receiver.latest_sequence_valid
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.new_count,
        3U
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.stale_count,
        4U
    );
}

TEST(MotionLifecycleCoordinatorTest, StartFromNoSessionBeginsStarting)
{
    MotionLifecycleCoordinator coordinator{};

    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &transaction
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.latest_sequence_valid
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.latest_sequence,
        42U
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &transaction
        )
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &transaction
        )
    );

    EXPECT_FALSE(action.send_response);

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );
}

TEST(MotionLifecycleCoordinatorTest, PendingRetryRepeatsAckWithoutRestartingOperation)
{
    MotionLifecycleCoordinator coordinator{};

    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction first_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &transaction
        );

    ASSERT_TRUE(first_action.send_ack);

    ASSERT_EQ(
        first_action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    const MotionLifecycleAction retry_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &transaction
        );

    EXPECT_TRUE(retry_action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &retry_action.ack_transaction,
            &transaction
        )
    );

    EXPECT_FALSE(retry_action.send_response);

    EXPECT_EQ(
        retry_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &transaction
        )
    );
}

TEST(MotionLifecycleCoordinatorTest, SuccessfulStartOperationActivatesSession)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &transaction
        );

    ASSERT_TRUE(start_action.send_ack);
    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    const MotionLifecycleAction completion_action =
        complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(completion_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion_action.response_transaction,
            &transaction
        )
    );

    EXPECT_EQ(
        completion_action.response_result,
        MOTION_RESPONSE_OK
    );

    EXPECT_FALSE(completion_action.send_ack);

    EXPECT_EQ(
        completion_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );
}

TEST(MotionLifecycleCoordinatorTest, FailedStartOperationInvalidatesSession)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &transaction
    );

    const MotionLifecycleAction completion_action =
        complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_FAILED
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(completion_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion_action.response_transaction,
            &transaction
        )
    );

    EXPECT_EQ(
        completion_action.response_result,
        MOTION_RESPONSE_STOP_FAILED
    );
}

TEST(MotionLifecycleCoordinatorTest, CompletedRetryReplaysAckAndCachedResponse)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    const MotionLifecycleAction retry_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &transaction
        );

    EXPECT_TRUE(retry_action.send_ack);
    EXPECT_TRUE(
        motion_transaction_equal(
            &retry_action.ack_transaction,
            &transaction
        )
    );

    EXPECT_TRUE(retry_action.send_response);
    EXPECT_TRUE(
        motion_transaction_equal(
            &retry_action.response_transaction,
            &transaction
        )
    );

    EXPECT_EQ(
        retry_action.response_result,
        MOTION_RESPONSE_OK
    );

    EXPECT_EQ(
        retry_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );
}

TEST(MotionLifecycleCoordinatorTest, StartForAlreadyActiveSessionReturnsAlreadyActive)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction first_start{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &first_start
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    const MotionTransaction second_start{
        .sequence = 43U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &second_start
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(action.send_ack);
    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &second_start
        )
    );

    EXPECT_TRUE(action.send_response);
    EXPECT_TRUE(
        motion_transaction_equal(
            &action.response_transaction,
            &second_start
        )
    );

    EXPECT_EQ(
        action.response_result,
        MOTION_RESPONSE_ALREADY_ACTIVE
    );

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &second_start
        )
    );
}

TEST(MotionLifecycleCoordinatorTest, InvalidReliableStateProducesNoAction)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    coordinator.reliable_receiver.latest_sequence_valid = true;
    coordinator.reliable_receiver.latest_sequence = 42U;

    const MotionTransaction transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &transaction
        );

    EXPECT_FALSE(action.send_ack);
    EXPECT_FALSE(action.send_response);

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );
}

TEST(MotionLifecycleCoordinatorTest, StartForDifferentActiveSessionReplacesSession)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction first_start{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &first_start
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    ASSERT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    const MotionTransaction second_start{
        .sequence = 43U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &second_start
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(108)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &second_start
        )
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &second_start
        )
    );

    EXPECT_FALSE(action.send_response);

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );
}

TEST(MotionLifecycleCoordinatorTest, EndActiveSessionBeginsEnding)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    ASSERT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    const MotionTransaction end_transaction{
        .sequence = 43U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &end_transaction
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &end_transaction
        )
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &end_transaction
        )
    );

    EXPECT_FALSE(action.send_response);

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );
}

TEST(MotionLifecycleCoordinatorTest, SuccessfulEndOperationRemovesSession)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    const MotionTransaction end_transaction{
        .sequence = 43U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &end_transaction
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    const MotionLifecycleAction completion_action =
        complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(completion_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion_action.response_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        completion_action.response_result,
        MOTION_RESPONSE_OK
    );

    EXPECT_EQ(
        completion_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );
}

TEST(MotionLifecycleCoordinatorTest, FailedEndOperationRemovesSession)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    const MotionTransaction end_transaction{
        .sequence = 43U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &end_transaction
    );

    const MotionLifecycleAction completion_action =
        complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_FAILED
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(completion_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion_action.response_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        completion_action.response_result,
        MOTION_RESPONSE_STOP_FAILED
    );
}

TEST(MotionLifecycleCoordinatorTest, EndForDifferentActiveSessionReturnsSessionMismatch)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    ASSERT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    const MotionTransaction end_transaction{
        .sequence = 43U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &end_transaction
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &end_transaction
        )
    );

    EXPECT_TRUE(action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.response_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        action.response_result,
        MOTION_RESPONSE_SESSION_MISMATCH
    );

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &end_transaction
        )
    );
}

TEST(MotionLifecycleCoordinatorTest, EndWithoutSessionReturnsAlreadyEnded)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction end_transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &end_transaction
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &end_transaction
        )
    );

    EXPECT_TRUE(action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.response_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        action.response_result,
        MOTION_RESPONSE_ALREADY_ENDED
    );

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &end_transaction
        )
    );
}

TEST(MotionLifecycleCoordinatorTest, NewStartWhileStartingSupersedesPreviousStart)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction first_start{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction first_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &first_start
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    ASSERT_EQ(
        first_action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    const MotionTransaction second_start{
        .sequence = 11U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction second_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &second_start
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(108)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &second_start
        )
    );

    EXPECT_TRUE(second_action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &second_action.ack_transaction,
            &second_start
        )
    );

    EXPECT_TRUE(second_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &second_action.response_transaction,
            &first_start
        )
    );

    EXPECT_EQ(
        second_action.response_result,
        MOTION_RESPONSE_SUPERSEDED
    );

    EXPECT_EQ(
        second_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );
}

TEST(MotionLifecycleCoordinatorTest, EndWhileStartingSupersedesStart)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    ASSERT_EQ(
        start_action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    const MotionTransaction end_transaction{
        .sequence = 11U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction end_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &end_transaction
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &end_transaction
        )
    );

    EXPECT_TRUE(end_action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &end_action.ack_transaction,
            &end_transaction
        )
    );

    EXPECT_TRUE(end_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &end_action.response_transaction,
            &start_transaction
        )
    );

    EXPECT_EQ(
        end_action.response_result,
        MOTION_RESPONSE_SUPERSEDED
    );

    EXPECT_EQ(
        end_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );
}

TEST(MotionLifecycleCoordinatorTest, EndForDifferentSessionWhileStartingReturnsSessionMismatch)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    ASSERT_EQ(
        start_action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    const MotionTransaction end_transaction{
        .sequence = 11U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction end_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &end_transaction
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &start_transaction
        )
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.terminal_result,
        MOTION_RESPONSE_SESSION_MISMATCH
    );

    EXPECT_TRUE(end_action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &end_action.ack_transaction,
            &end_transaction
        )
    );

    EXPECT_TRUE(end_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &end_action.response_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        end_action.response_result,
        MOTION_RESPONSE_SESSION_MISMATCH
    );

    EXPECT_EQ(
        end_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );
}

TEST(MotionLifecycleCoordinatorTest, StartWhileEndingReturnsBusyStopping)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    const MotionTransaction end_transaction{
        .sequence = 20U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &end_transaction
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    ASSERT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    const MotionTransaction second_start{
        .sequence = 21U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &second_start
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &end_transaction
        )
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &second_start
        )
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.terminal_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &second_start
        )
    );

    EXPECT_TRUE(action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.response_transaction,
            &second_start
        )
    );

    EXPECT_EQ(
        action.response_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );
}

TEST(MotionLifecycleCoordinatorTest, NewEndWhileEndingSupersedesPreviousEnd)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    const MotionTransaction first_end{
        .sequence = 20U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction first_end_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &first_end
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    ASSERT_EQ(
        first_end_action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    const MotionTransaction second_end{
        .sequence = 21U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction second_end_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &second_end
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &second_end
        )
    );

    EXPECT_TRUE(second_end_action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &second_end_action.ack_transaction,
            &second_end
        )
    );

    EXPECT_TRUE(second_end_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &second_end_action.response_transaction,
            &first_end
        )
    );

    EXPECT_EQ(
        second_end_action.response_result,
        MOTION_RESPONSE_SUPERSEDED
    );

    EXPECT_EQ(
        second_end_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );
}

TEST(MotionLifecycleCoordinatorTest, EndForDifferentSessionWhileEndingReturnsSessionMismatch)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    const MotionTransaction first_end{
        .sequence = 20U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &first_end
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    const MotionTransaction wrong_end{
        .sequence = 21U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &wrong_end
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &first_end
        )
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &wrong_end
        )
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.terminal_result,
        MOTION_RESPONSE_SESSION_MISMATCH
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &wrong_end
        )
    );

    EXPECT_TRUE(action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.response_transaction,
            &wrong_end
        )
    );

    EXPECT_EQ(
        action.response_result,
        MOTION_RESPONSE_SESSION_MISMATCH
    );

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );
}

TEST(MotionLifecycleCoordinatorTest, UnknownCommandReturnsInvalidCommand)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction transaction{
        .sequence = 30U,
        .command = static_cast<MotionLifecycleCommandType>(0x7FU),
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &transaction
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.ack_transaction,
            &transaction
        )
    );

    EXPECT_TRUE(action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &action.response_transaction,
            &transaction
        )
    );

    EXPECT_EQ(
        action.response_result,
        MOTION_RESPONSE_INVALID_COMMAND
    );

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &transaction
        )
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.terminal_result,
        MOTION_RESPONSE_INVALID_COMMAND
    );
}

TEST(MotionLifecycleCoordinatorTest, CompletingOlderEndPreservesNewerBusyStoppingTerminal)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_transaction
    );

    complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    const MotionTransaction end_transaction{
        .sequence = 20U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &end_transaction
    );

    const MotionTransaction new_start{
        .sequence = 21U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction busy_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &new_start
        );

    ASSERT_EQ(
        busy_action.response_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    ASSERT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    ASSERT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    const MotionLifecycleAction completion_action =
        complete_active_operation(
            &coordinator,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(completion_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion_action.response_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        completion_action.response_result,
        MOTION_RESPONSE_OK
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &new_start
        )
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.terminal_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );
}

TEST(MotionLifecycleCoordinatorTest, LateCompletionAfterResetDoesNotCompleteNewOperation)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_a{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action_a =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_a
        );

    ASSERT_EQ(
        action_a.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    ASSERT_NE(action_a.operation_id, 0U);

    const uint32_t operation_a_id =
        action_a.operation_id;

    motion_lifecycle_coordinator_reset(
        &coordinator
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        0U
    );

    const MotionTransaction start_b{
        .sequence = 20U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction action_b =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_b
        );

    ASSERT_EQ(
        action_b.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    ASSERT_NE(action_b.operation_id, 0U);

    EXPECT_NE(
        action_b.operation_id,
        operation_a_id
    );

    const uint32_t operation_b_id =
        action_b.operation_id;

    ASSERT_EQ(
        coordinator.active_operation_id,
        operation_b_id
    );

    /*
     * Delayed callback from the operation that existed
     * before reset().
     */
    const MotionLifecycleAction late_completion =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            operation_a_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    EXPECT_FALSE(late_completion.send_ack);
    EXPECT_FALSE(late_completion.send_response);

    EXPECT_EQ(
        late_completion.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    /*
     * START B must still be waiting for its own
     * physical operation.
     */
    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(108)
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        operation_b_id
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &start_b
        )
    );

    /*
     * The real completion for B must still work.
     */
    const MotionLifecycleAction completion_b =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            operation_b_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(108)
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        0U
    );

    EXPECT_TRUE(completion_b.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion_b.response_transaction,
            &start_b
        )
    );

    EXPECT_EQ(
        completion_b.response_result,
        MOTION_RESPONSE_OK
    );
}

TEST(MotionLifecycleCoordinatorTest, StartPreemptionKeepsPhysicalOperationAndCompletesNewOwner)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_a{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action_a =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_a
        );

    ASSERT_EQ(
        action_a.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    ASSERT_NE(action_a.operation_id, 0U);

    const uint32_t operation_id =
        action_a.operation_id;

    ASSERT_EQ(
        coordinator.active_operation_id,
        operation_id
    );

    const MotionTransaction start_b{
        .sequence = 11U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction preemption_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_b
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(108)
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        operation_id
    );

    EXPECT_EQ(
        preemption_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(
        preemption_action.operation_id,
        0U
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &start_b
        )
    );

    const MotionLifecycleAction completion =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            operation_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(108)
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        0U
    );

    EXPECT_TRUE(completion.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion.response_transaction,
            &start_b
        )
    );

    EXPECT_EQ(
        completion.response_result,
        MOTION_RESPONSE_OK
    );
}

TEST(MotionLifecycleCoordinatorTest, EndPreemptionWhileStartingKeepsPhysicalOperation)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    ASSERT_EQ(
        start_action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    ASSERT_NE(start_action.operation_id, 0U);

    const uint32_t operation_id =
        start_action.operation_id;

    const MotionTransaction end_transaction{
        .sequence = 11U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction preemption_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &end_transaction
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        operation_id
    );

    EXPECT_EQ(
        preemption_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(
        preemption_action.operation_id,
        0U
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &end_transaction
        )
    );

    const MotionLifecycleAction completion =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            operation_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        0U
    );

    EXPECT_TRUE(completion.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion.response_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        completion.response_result,
        MOTION_RESPONSE_OK
    );
}

TEST(MotionLifecycleCoordinatorTest, EndPreemptionWhileEndingKeepsPhysicalOperation)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    motion_lifecycle_coordinator_complete_operation(
        &coordinator,
        start_action.operation_id,
        MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    const MotionTransaction first_end{
        .sequence = 20U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction first_end_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &first_end
        );

    ASSERT_EQ(
        first_end_action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    ASSERT_NE(first_end_action.operation_id, 0U);

    const uint32_t operation_id =
        first_end_action.operation_id;

    const MotionTransaction second_end{
        .sequence = 21U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction preemption_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &second_end
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        operation_id
    );

    EXPECT_EQ(
        preemption_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(
        preemption_action.operation_id,
        0U
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &second_end
        )
    );

    const MotionLifecycleAction completion =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            operation_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        0U
    );

    EXPECT_TRUE(completion.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion.response_transaction,
            &second_end
        )
    );

    EXPECT_EQ(
        completion.response_result,
        MOTION_RESPONSE_OK
    );
}

TEST(MotionLifecycleCoordinatorTest, FailedStartPreemptionFailsNewOwner)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_a{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action_a =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_a
        );

    ASSERT_NE(action_a.operation_id, 0U);

    const uint32_t operation_id =
        action_a.operation_id;

    const MotionTransaction start_b{
        .sequence = 11U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &start_b
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    ASSERT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &start_b
        )
    );

    const MotionLifecycleAction completion =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            operation_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_FAILED
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        0U
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(completion.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion.response_transaction,
            &start_b
        )
    );

    EXPECT_EQ(
        completion.response_result,
        MOTION_RESPONSE_STOP_FAILED
    );
}

TEST(MotionLifecycleCoordinatorTest, FailedEndPreemptionWhileStartingFailsNewOwner)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    ASSERT_NE(start_action.operation_id, 0U);

    const uint32_t operation_id =
        start_action.operation_id;

    const MotionTransaction end_transaction{
        .sequence = 11U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &end_transaction
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    ASSERT_EQ(
        coordinator.active_operation_id,
        operation_id
    );

    ASSERT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &end_transaction
        )
    );

    const MotionLifecycleAction completion =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            operation_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_FAILED
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        0U
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(completion.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion.response_transaction,
            &end_transaction
        )
    );

    EXPECT_EQ(
        completion.response_result,
        MOTION_RESPONSE_STOP_FAILED
    );
}

TEST(MotionLifecycleCoordinatorTest, FailedEndPreemptionWhileEndingFailsNewOwner)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    motion_lifecycle_coordinator_complete_operation(
        &coordinator,
        start_action.operation_id,
        MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    const MotionTransaction first_end{
        .sequence = 20U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction first_end_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &first_end
        );

    ASSERT_NE(first_end_action.operation_id, 0U);

    const uint32_t operation_id =
        first_end_action.operation_id;

    const MotionTransaction second_end{
        .sequence = 21U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    motion_lifecycle_coordinator_handle_transaction(
        &coordinator,
        &second_end
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ENDING
    );

    ASSERT_EQ(
        coordinator.active_operation_id,
        operation_id
    );

    ASSERT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &second_end
        )
    );

    const MotionLifecycleAction completion =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            operation_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_FAILED
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        0U
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        0U
    );

    EXPECT_FALSE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(completion.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &completion.response_transaction,
            &second_end
        )
    );

    EXPECT_EQ(
        completion.response_result,
        MOTION_RESPONSE_STOP_FAILED
    );
}

TEST(MotionLifecycleCoordinatorTest, NewerTerminalCanBeReplayedAfterOlderPendingCompletes)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    motion_lifecycle_coordinator_complete_operation(
        &coordinator,
        start_action.operation_id,
        MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    const MotionTransaction end_transaction{
        .sequence = 20U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction end_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &end_transaction
        );

    ASSERT_NE(end_action.operation_id, 0U);

    const uint32_t operation_id =
        end_action.operation_id;

    const MotionTransaction new_start{
        .sequence = 21U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    const MotionLifecycleAction busy_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &new_start
        );

    ASSERT_TRUE(busy_action.send_response);

    ASSERT_EQ(
        busy_action.response_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    motion_lifecycle_coordinator_complete_operation(
        &coordinator,
        operation_id,
        MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
    );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );

    ASSERT_TRUE(
        coordinator.reliable_receiver.terminal_valid
    );

    ASSERT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.terminal_transaction,
            &new_start
        )
    );

    ASSERT_EQ(
        coordinator.reliable_receiver.terminal_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    const MotionLifecycleAction retry_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &new_start
        );

    EXPECT_TRUE(retry_action.send_ack);

    EXPECT_TRUE(
        motion_transaction_equal(
            &retry_action.ack_transaction,
            &new_start
        )
    );

    EXPECT_TRUE(retry_action.send_response);

    EXPECT_TRUE(
        motion_transaction_equal(
            &retry_action.response_transaction,
            &new_start
        )
    );

    EXPECT_EQ(
        retry_action.response_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    EXPECT_EQ(
        retry_action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(
        retry_action.operation_id,
        0U
    );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION
    );
}

TEST(MotionLifecycleCoordinatorTest, CollisionDoesNotChangeLifecycleState)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    ASSERT_NE(start_action.operation_id, 0U);

    const uint32_t operation_id =
        start_action.operation_id;

    /*
     * Same sequence, different content:
     * this is not a retry — it is a collision.
     */
    const MotionTransaction collision{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &collision
        );

    EXPECT_FALSE(action.send_ack);
    EXPECT_FALSE(action.send_response);

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(action.operation_id, 0U);

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        operation_id
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &start_transaction
        )
    );
}

TEST(MotionLifecycleCoordinatorTest, StaleTransactionDoesNotChangeLifecycleState)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    ASSERT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    ASSERT_NE(start_action.operation_id, 0U);

    const uint32_t operation_id =
        start_action.operation_id;

    const MotionTransaction stale_transaction{
        .sequence = 9U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &stale_transaction
        );

    EXPECT_FALSE(action.send_ack);
    EXPECT_FALSE(action.send_response);

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_NONE
    );

    EXPECT_EQ(
        action.operation_id,
        0U
    );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_EQ(
        coordinator.active_operation_id,
        operation_id
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_TRUE(
        motion_transaction_equal(
            &coordinator.reliable_receiver.pending_transaction,
            &start_transaction
        )
    );
}