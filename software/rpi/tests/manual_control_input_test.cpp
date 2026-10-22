#include <gtest/gtest.h>

#include "manual_control_input.h"

TEST(ManualControlInputTest, NormalizesBoundariesAndClamps)
{
    EXPECT_DOUBLE_EQ(normalize_manual_stick(0U), -1.0);
    EXPECT_DOUBLE_EQ(normalize_manual_stick(174U), -1.0);

    EXPECT_DOUBLE_EQ(normalize_manual_stick(967U), 0.0);
    EXPECT_DOUBLE_EQ(normalize_manual_stick(992U), 0.0);
    EXPECT_DOUBLE_EQ(normalize_manual_stick(1017U), 0.0);

    EXPECT_DOUBLE_EQ(normalize_manual_stick(1811U), 1.0);
    EXPECT_DOUBLE_EQ(normalize_manual_stick(2047U), 1.0);
}

TEST(ManualControlInputTest, ScalesOutsideDeadband)
{
    EXPECT_DOUBLE_EQ(
        normalize_manual_stick(1414U),
        0.5);

    EXPECT_DOUBLE_EQ(
        normalize_manual_stick(570U),
        -397.0 / 793.0);
}

TEST(ManualControlInputTest, MixedManualSticks)
{
    ManualWheelDemand results = mix_manual_sticks(0.0, 0.0);
    EXPECT_DOUBLE_EQ(results.left, 0.0);
    EXPECT_DOUBLE_EQ(results.right, 0.0);

    results = mix_manual_sticks(1.0, 0.0);
    EXPECT_DOUBLE_EQ(results.left, 1.0);
    EXPECT_DOUBLE_EQ(results.right, 1.0);

    results = mix_manual_sticks(-1.0, 0.0);
    EXPECT_DOUBLE_EQ(results.left, -1.0);
    EXPECT_DOUBLE_EQ(results.right, -1.0);

    results = mix_manual_sticks(0.0, 1.0);
    EXPECT_DOUBLE_EQ(results.left, 1.0);
    EXPECT_DOUBLE_EQ(results.right, -1.0);

    results = mix_manual_sticks(1.0, 1.0);
    EXPECT_DOUBLE_EQ(results.left, 1.0);
    EXPECT_DOUBLE_EQ(results.right, 0.0);

    results = mix_manual_sticks(0.5, -1.0);
    EXPECT_DOUBLE_EQ(results.left, -1.0/3.0);
    EXPECT_DOUBLE_EQ(results.right, 1.0);
}
