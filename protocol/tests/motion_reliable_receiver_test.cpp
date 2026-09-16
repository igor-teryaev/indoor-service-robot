#include <gtest/gtest.h>

#include <cstdint>

#include "motion_reliable_receiver.h"

namespace
{

MotionTransaction make_transaction(
    uint16_t sequence,
    MotionLifecycleCommandType command,
    uint32_t motion_session_id)
{
    return MotionTransaction{
        .sequence = sequence,
        .command = command,
        .motion_session_id = motion_session_id
    };
}

}

TEST(MotionReliableReceiverTest, RejectsInvalidArguments)
{
    MotionReliableReceiver receiver{};
    const MotionTransaction transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            nullptr,
            &transaction
        ),
        MOTION_RELIABLE_DECISION_INVALID
    );

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            nullptr
        ),
        MOTION_RELIABLE_DECISION_INVALID
    );
}

TEST(MotionReliableReceiverTest, FirstTransactionIsNew)
{
    const MotionReliableReceiver receiver{};

    const MotionTransaction transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &transaction
        ),
        MOTION_RELIABLE_DECISION_NEW
    );
}

TEST(MotionReliableReceiverTest, NewerTransactionIsNew)
{
    MotionReliableReceiver receiver{};
    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    const MotionTransaction transaction =
        make_transaction(
            43U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(107)
        );

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &transaction
        ),
        MOTION_RELIABLE_DECISION_NEW
    );
}

TEST(MotionReliableReceiverTest, OlderTransactionIsStale)
{
    MotionReliableReceiver receiver{};
    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    const MotionTransaction transaction =
        make_transaction(
            41U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &transaction
        ),
        MOTION_RELIABLE_DECISION_STALE
    );
}

TEST(MotionReliableReceiverTest, SameCompletedTransactionIsCompletedRetry)
{
    MotionReliableReceiver receiver{};

    const MotionTransaction transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    receiver.terminal_valid = true;
    receiver.terminal_transaction = transaction;
    receiver.terminal_result = MOTION_RESPONSE_OK;

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &transaction
        ),
        MOTION_RELIABLE_DECISION_COMPLETED_RETRY
    );
}

TEST(MotionReliableReceiverTest, SameSequenceWithDifferentCompletedTransactionIsCollision)
{
    MotionReliableReceiver receiver{};

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    receiver.terminal_valid = true;
    receiver.terminal_transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    const MotionTransaction incoming =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(107)
        );

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &incoming
        ),
        MOTION_RELIABLE_DECISION_COLLISION
    );
}

TEST(MotionReliableReceiverTest, SamePendingTransactionIsPendingRetry)
{
    MotionReliableReceiver receiver{};

    const MotionTransaction transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    receiver.pending_valid = true;
    receiver.pending_transaction = transaction;

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &transaction
        ),
        MOTION_RELIABLE_DECISION_PENDING_RETRY
    );
}

TEST(MotionReliableReceiverTest, SameSequenceWithDifferentPendingTransactionIsCollision)
{
    MotionReliableReceiver receiver{};

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    receiver.pending_valid = true;
    receiver.pending_transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    const MotionTransaction incoming =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(108)
        );

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &incoming
        ),
        MOTION_RELIABLE_DECISION_COLLISION
    );
}

TEST(MotionReliableReceiverTest, MissingLatestTransactionIdentityIsInvalid)
{
    MotionReliableReceiver receiver{};

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    const MotionTransaction incoming =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &incoming
        ),
        MOTION_RELIABLE_DECISION_INVALID
    );
}

TEST(MotionReliableReceiverTest, OlderPendingTransactionIsStaleAfterNewerTerminalTransaction)
{
    MotionReliableReceiver receiver{};

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 11U;

    receiver.pending_valid = true;
    receiver.pending_transaction =
        make_transaction(
            10U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(107)
        );

    receiver.terminal_valid = true;
    receiver.terminal_transaction =
        make_transaction(
            11U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(108)
        );

    receiver.terminal_result =
        MOTION_RESPONSE_BUSY_STOPPING;

    EXPECT_EQ(
        motion_reliable_receiver_classify(
            &receiver,
            &receiver.pending_transaction
        ),
        MOTION_RELIABLE_DECISION_STALE
    );
}

TEST(MotionReliableReceiverTest, InitClearsStateAndDiagnostics)
{
    MotionReliableReceiver receiver{};

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    receiver.pending_valid = true;
    receiver.pending_transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    receiver.terminal_valid = true;
    receiver.terminal_transaction =
        make_transaction(
            41U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(106)
        );

    receiver.terminal_result = MOTION_RESPONSE_BUSY_STOPPING;

    receiver.new_count = 1U;
    receiver.pending_retry_count = 2U;
    receiver.completed_retry_count = 3U;
    receiver.stale_count = 4U;
    receiver.collision_count = 5U;

    motion_reliable_receiver_init(&receiver);

    EXPECT_FALSE(receiver.latest_sequence_valid);
    EXPECT_EQ(receiver.latest_sequence, 0U);

    EXPECT_FALSE(receiver.pending_valid);
    EXPECT_FALSE(receiver.terminal_valid);

    EXPECT_EQ(receiver.new_count, 0U);
    EXPECT_EQ(receiver.pending_retry_count, 0U);
    EXPECT_EQ(receiver.completed_retry_count, 0U);
    EXPECT_EQ(receiver.stale_count, 0U);
    EXPECT_EQ(receiver.collision_count, 0U);
}

TEST(MotionReliableReceiverTest, ResetClearsStateButPreservesDiagnostics)
{
    MotionReliableReceiver receiver{};

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    receiver.pending_valid = true;
    receiver.pending_transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    receiver.terminal_valid = true;
    receiver.terminal_transaction =
        make_transaction(
            41U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(106)
        );

    receiver.terminal_result = MOTION_RESPONSE_BUSY_STOPPING;

    receiver.new_count = 1U;
    receiver.pending_retry_count = 2U;
    receiver.completed_retry_count = 3U;
    receiver.stale_count = 4U;
    receiver.collision_count = 5U;

    motion_reliable_receiver_reset(&receiver);

    EXPECT_FALSE(receiver.latest_sequence_valid);
    EXPECT_EQ(receiver.latest_sequence, 0U);

    EXPECT_FALSE(receiver.pending_valid);
    EXPECT_EQ(receiver.pending_transaction.sequence, 0U);
    EXPECT_EQ(receiver.pending_transaction.command, 0U);
    EXPECT_EQ(receiver.pending_transaction.motion_session_id, 0U);

    EXPECT_FALSE(receiver.terminal_valid);
    EXPECT_EQ(receiver.terminal_transaction.sequence, 0U);
    EXPECT_EQ(receiver.terminal_transaction.command, 0U);
    EXPECT_EQ(receiver.terminal_transaction.motion_session_id, 0U);
    EXPECT_EQ(receiver.terminal_result, 0U);

    EXPECT_EQ(receiver.new_count, 1U);
    EXPECT_EQ(receiver.pending_retry_count, 2U);
    EXPECT_EQ(receiver.completed_retry_count, 3U);
    EXPECT_EQ(receiver.stale_count, 4U);
    EXPECT_EQ(receiver.collision_count, 5U);
}

TEST(MotionReliableReceiverTest, BeginPendingMakesTransactionLatestAndClearsTerminal)
{
    MotionReliableReceiver receiver{};

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 41U;

    receiver.terminal_valid = true;
    receiver.terminal_transaction =
        make_transaction(
            41U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(106)
        );

    receiver.terminal_result =
        MOTION_RESPONSE_BUSY_STOPPING;

    const MotionTransaction transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    motion_reliable_receiver_begin_pending(
        &receiver,
        &transaction
    );

    EXPECT_TRUE(receiver.latest_sequence_valid);
    EXPECT_EQ(receiver.latest_sequence, 42U);

    EXPECT_TRUE(receiver.pending_valid);
    EXPECT_TRUE(
        motion_transaction_equal(
            &receiver.pending_transaction,
            &transaction
        )
    );

    EXPECT_FALSE(receiver.terminal_valid);
    EXPECT_EQ(receiver.terminal_transaction.sequence, 0U);
    EXPECT_EQ(receiver.terminal_transaction.command, 0U);
    EXPECT_EQ(
        receiver.terminal_transaction.motion_session_id,
        0U
    );
    EXPECT_EQ(receiver.terminal_result, 0U);
}

TEST(MotionReliableReceiverTest, BeginPendingSupportsAliasedTerminalTransaction)
{
    MotionReliableReceiver receiver{};

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    receiver.terminal_valid = true;
    receiver.terminal_transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    receiver.terminal_result =
        MOTION_RESPONSE_OK;

    motion_reliable_receiver_begin_pending(
        &receiver,
        &receiver.terminal_transaction
    );

    EXPECT_TRUE(receiver.latest_sequence_valid);
    EXPECT_EQ(receiver.latest_sequence, 42U);

    EXPECT_TRUE(receiver.pending_valid);

    EXPECT_EQ(
        receiver.pending_transaction.sequence,
        42U
    );
    EXPECT_EQ(
        receiver.pending_transaction.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION
    );
    EXPECT_EQ(
        receiver.pending_transaction.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_FALSE(receiver.terminal_valid);
    EXPECT_EQ(receiver.terminal_transaction.sequence, 0U);
    EXPECT_EQ(receiver.terminal_transaction.command, 0U);
    EXPECT_EQ(
        receiver.terminal_transaction.motion_session_id,
        0U
    );
}

TEST(MotionReliableReceiverTest, ImmediateTerminalPreservesOlderPendingTransaction)
{
    MotionReliableReceiver receiver{};

    const MotionTransaction pending =
        make_transaction(
            10U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(107)
        );

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 10U;
    receiver.pending_valid = true;
    receiver.pending_transaction = pending;

    const MotionTransaction incoming =
        make_transaction(
            11U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(108)
        );

    motion_reliable_receiver_record_immediate_terminal(
        &receiver,
        &incoming,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    EXPECT_TRUE(receiver.latest_sequence_valid);
    EXPECT_EQ(receiver.latest_sequence, 11U);

    EXPECT_TRUE(receiver.pending_valid);
    EXPECT_TRUE(
        motion_transaction_equal(
            &receiver.pending_transaction,
            &pending
        )
    );

    EXPECT_TRUE(receiver.terminal_valid);
    EXPECT_TRUE(
        motion_transaction_equal(
            &receiver.terminal_transaction,
            &incoming
        )
    );

    EXPECT_EQ(
        receiver.terminal_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );
}

TEST(MotionReliableReceiverTest, CompletingLatestPendingCachesTerminalResult)
{
    MotionReliableReceiver receiver{};

    const MotionTransaction transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;
    receiver.pending_valid = true;
    receiver.pending_transaction = transaction;

    const bool completed =
        motion_reliable_receiver_complete_pending(
            &receiver,
            &transaction,
            MOTION_RESPONSE_OK
        );

    EXPECT_TRUE(completed);

    EXPECT_FALSE(receiver.pending_valid);
    EXPECT_EQ(receiver.pending_transaction.sequence, 0U);
    EXPECT_EQ(receiver.pending_transaction.command, 0U);
    EXPECT_EQ(receiver.pending_transaction.motion_session_id, 0U);

    EXPECT_TRUE(receiver.terminal_valid);
    EXPECT_TRUE(
        motion_transaction_equal(
            &receiver.terminal_transaction,
            &transaction
        )
    );
    EXPECT_EQ(
        receiver.terminal_result,
        MOTION_RESPONSE_OK
    );

    EXPECT_TRUE(receiver.latest_sequence_valid);
    EXPECT_EQ(receiver.latest_sequence, 42U);
}

TEST(MotionReliableReceiverTest, CompletingOlderPendingPreservesNewerTerminal)
{
    MotionReliableReceiver receiver{};

    const MotionTransaction old_pending =
        make_transaction(
            10U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(107)
        );

    const MotionTransaction newer_terminal =
        make_transaction(
            11U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(108)
        );

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 11U;

    receiver.pending_valid = true;
    receiver.pending_transaction = old_pending;

    receiver.terminal_valid = true;
    receiver.terminal_transaction = newer_terminal;
    receiver.terminal_result =
        MOTION_RESPONSE_BUSY_STOPPING;

    const bool completed =
        motion_reliable_receiver_complete_pending(
            &receiver,
            &old_pending,
            MOTION_RESPONSE_OK
        );

    EXPECT_TRUE(completed);

    EXPECT_FALSE(receiver.pending_valid);

    EXPECT_TRUE(receiver.terminal_valid);
    EXPECT_TRUE(
        motion_transaction_equal(
            &receiver.terminal_transaction,
            &newer_terminal
        )
    );
    EXPECT_EQ(
        receiver.terminal_result,
        MOTION_RESPONSE_BUSY_STOPPING
    );

    EXPECT_EQ(receiver.latest_sequence, 11U);
}

TEST(MotionReliableReceiverTest, CompletingDifferentTransactionDoesNotChangeState)
{
    MotionReliableReceiver receiver{};

    const MotionTransaction pending =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    const MotionTransaction different =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(108)
        );

    receiver.latest_sequence_valid = true;
    receiver.latest_sequence = 42U;

    receiver.pending_valid = true;
    receiver.pending_transaction = pending;

    const bool completed =
        motion_reliable_receiver_complete_pending(
            &receiver,
            &different,
            MOTION_RESPONSE_OK
        );

    EXPECT_FALSE(completed);

    EXPECT_TRUE(receiver.pending_valid);
    EXPECT_TRUE(
        motion_transaction_equal(
            &receiver.pending_transaction,
            &pending
        )
    );

    EXPECT_FALSE(receiver.terminal_valid);

    EXPECT_TRUE(receiver.latest_sequence_valid);
    EXPECT_EQ(receiver.latest_sequence, 42U);
}