#include <gtest/gtest.h>

#include <cstdint>

#include "motion_sequence.h"

TEST(MotionSequenceTest, SameSequenceIsSame)
{
    EXPECT_EQ(
        motion_sequence_classify(100U, 100U),
        MOTION_SEQUENCE_SAME
    );
}

TEST(MotionSequenceTest, NextSequenceIsNewer)
{
    EXPECT_EQ(
        motion_sequence_classify(100U, 101U),
        MOTION_SEQUENCE_NEWER
    );
}

TEST(MotionSequenceTest, PreviousSequenceIsStale)
{
    EXPECT_EQ(
        motion_sequence_classify(100U, 99U),
        MOTION_SEQUENCE_STALE
    );
}

TEST(MotionSequenceTest, WrapAroundSequenceIsNewer)
{
    EXPECT_EQ(
        motion_sequence_classify(UINT16_MAX, 0U),
        MOTION_SEQUENCE_NEWER
    );

    EXPECT_EQ(
        motion_sequence_classify(0U, UINT16_MAX),
        MOTION_SEQUENCE_STALE
    );
}

TEST(MotionSequenceTest, HalfRangeBoundaryIsStale)
{
    EXPECT_EQ(
        motion_sequence_classify(
            0U,
            UINT16_C(0x7FFF)
        ),
        MOTION_SEQUENCE_NEWER
    );

    EXPECT_EQ(
        motion_sequence_classify(
            0U,
            UINT16_C(0x8000)
        ),
        MOTION_SEQUENCE_STALE
    );
}