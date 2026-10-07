#include <gtest/gtest.h>

extern "C"
{
#include "motion_stop.h"
#include "wheel_encoder.h"
}

namespace
{
    WheelEncoderCounts encoder_counts{0U, 0U};
}

extern "C" WheelEncoderCounts wheel_encoder_read(void)
{
    return encoder_counts;
}

class MotionStopTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        motion_stop_cancel();
        encoder_counts = {0U, 0U};
    }
};

TEST_F(MotionStopTest, CompletesOnceAfterContinuousSettleTime)
{
    ASSERT_TRUE(motion_stop_begin(7U, 100U));

    EXPECT_FALSE(motion_stop_update(299U).completed);

    const MotionStopCompletion completion = motion_stop_update(300U);
    EXPECT_TRUE(completion.completed);
    EXPECT_EQ(completion.operation_id, 7U);
    EXPECT_EQ(completion.result, MOTION_STOP_RESULT_SUCCESS);

    EXPECT_FALSE(motion_stop_update(301U).completed);
}

TEST_F(MotionStopTest, FailsAtOverallTimeoutAfterLateMovement)
{
    ASSERT_TRUE(motion_stop_begin(8U, 0U));

    encoder_counts.left = 1U;
    EXPECT_FALSE(motion_stop_update(700U).completed);

    encoder_counts.right = 1U;
    EXPECT_FALSE(motion_stop_update(900U).completed);

    EXPECT_FALSE(motion_stop_update(999U).completed);

    const MotionStopCompletion completion = motion_stop_update(1000U);

    EXPECT_TRUE(completion.completed);
    EXPECT_EQ(completion.operation_id, 8U);
    EXPECT_EQ(completion.result, MOTION_STOP_RESULT_FAILED);
    EXPECT_FALSE(motion_stop_update(1001U).completed);
}

TEST_F(MotionStopTest, NewOperationReplacesPreviousOperation)
{
    ASSERT_TRUE(motion_stop_begin(10U, 0U));

    encoder_counts.left = 1U;
    ASSERT_TRUE(motion_stop_begin(11U, 50U));

    EXPECT_FALSE(motion_stop_update(249U).completed);

    const MotionStopCompletion completion =
        motion_stop_update(250U);

    EXPECT_TRUE(completion.completed);
    EXPECT_EQ(completion.operation_id, 11U);
    EXPECT_EQ(completion.result, MOTION_STOP_RESULT_SUCCESS);
}

TEST_F(MotionStopTest, ZeroOperationIdDoesNotPreemptActiveOperation)
{
    ASSERT_TRUE(motion_stop_begin(12U, 0U));
    EXPECT_FALSE(motion_stop_begin(0U, 50U));

    const MotionStopCompletion completion = motion_stop_update(200U);
    EXPECT_TRUE(completion.completed);
    EXPECT_EQ(completion.operation_id, 12U);
    EXPECT_EQ(completion.result, MOTION_STOP_RESULT_SUCCESS);
}

TEST_F(MotionStopTest, CancelSuppressesPendingCompletion)
{
    ASSERT_TRUE(motion_stop_begin(13U, 100U));
    motion_stop_cancel();

    const MotionStopCompletion completion = motion_stop_update(5000U);
    EXPECT_FALSE(completion.completed);
    EXPECT_EQ(completion.operation_id, 0U);
    EXPECT_EQ(completion.result, MOTION_STOP_RESULT_NONE);
}

TEST_F(MotionStopTest, CompletesAcrossTickCounterWraparound)
{
    const uint32_t start_ms = UINT32_MAX - 99U;
    ASSERT_TRUE(motion_stop_begin(14U, start_ms));
    EXPECT_FALSE(motion_stop_update(99U).completed);

    const MotionStopCompletion completion = motion_stop_update(100U);
    EXPECT_TRUE(completion.completed);
    EXPECT_EQ(completion.operation_id, 14U);
    EXPECT_EQ(completion.result, MOTION_STOP_RESULT_SUCCESS);
}