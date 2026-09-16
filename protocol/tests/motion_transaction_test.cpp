#include <gtest/gtest.h>

#include <cstdint>

#include "motion_transaction.h"

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

TEST(MotionTransactionTest, EqualTransactionsMatch)
{
    const MotionTransaction first =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    const MotionTransaction second = first;

    EXPECT_TRUE(
        motion_transaction_equal(&first, &second)
    );
}

TEST(MotionTransactionTest, DifferentSequenceDoesNotMatch)
{
    const MotionTransaction first =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    const MotionTransaction second =
        make_transaction(
            43U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    EXPECT_FALSE(
        motion_transaction_equal(&first, &second)
    );
}

TEST(MotionTransactionTest, DifferentCommandDoesNotMatch)
{
    const MotionTransaction first =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    const MotionTransaction second =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            UINT32_C(107)
        );

    EXPECT_FALSE(
        motion_transaction_equal(&first, &second)
    );
}

TEST(MotionTransactionTest, DifferentSessionIdDoesNotMatch)
{
    const MotionTransaction first =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    const MotionTransaction second =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(108)
        );

    EXPECT_FALSE(
        motion_transaction_equal(&first, &second)
    );
}

TEST(MotionTransactionTest, NullTransactionsDoNotMatch)
{
    const MotionTransaction transaction =
        make_transaction(
            42U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            UINT32_C(107)
        );

    EXPECT_FALSE(
        motion_transaction_equal(nullptr, &transaction)
    );

    EXPECT_FALSE(
        motion_transaction_equal(&transaction, nullptr)
    );

    EXPECT_FALSE(
        motion_transaction_equal(nullptr, nullptr)
    );
}