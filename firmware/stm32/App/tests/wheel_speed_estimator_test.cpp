#include <gtest/gtest.h>

#include <cstdint>

#include "wheel_speed_estimator.h"

TEST(WheelSpeedEstimatorTest, FirstSampleEstablishesBaseline)
{
    WheelSpeedEstimator estimator{};
    wheel_speed_estimator_init(&estimator);

    const auto result = wheel_speed_estimator_update(
        &estimator, {100U, 200U}, 1000U);

    EXPECT_FALSE(result.valid);
    EXPECT_FLOAT_EQ(result.left_mm_s, 0.0f);
    EXPECT_FLOAT_EQ(result.right_mm_s, 0.0f);
}

TEST(WheelSpeedEstimatorTest, CalculatesBothWheelSpeeds)
{
    WheelSpeedEstimator estimator{};
    wheel_speed_estimator_init(&estimator);

    wheel_speed_estimator_update(&estimator, {100U, 200U}, 1000U);

    const auto result = wheel_speed_estimator_update(
        &estimator, {200U, 250U}, 1100U);

    EXPECT_TRUE(result.valid);
    EXPECT_NEAR(result.left_mm_s, 215.2f, 0.1f);
    EXPECT_NEAR(result.right_mm_s, 107.6f, 0.1f);
}

TEST(WheelSpeedEstimatorTest, ZeroElapsedTimePreservesBaseline)
{
    WheelSpeedEstimator estimator{};
    wheel_speed_estimator_init(&estimator);

    wheel_speed_estimator_update(&estimator, {100U, 100U}, 1000U);

    const auto invalid = wheel_speed_estimator_update(
        &estimator, {150U, 150U}, 1000U);

    EXPECT_FALSE(invalid.valid);

    const auto result = wheel_speed_estimator_update(
        &estimator, {200U, 200U}, 1100U);

    EXPECT_TRUE(result.valid);
    EXPECT_NEAR(result.left_mm_s, 215.2f, 0.1f);
    EXPECT_NEAR(result.right_mm_s, 215.2f, 0.1f);
}

TEST(WheelSpeedEstimatorTest, HandlesEncoderCounterRollover)
{
    WheelSpeedEstimator estimator{};
    wheel_speed_estimator_init(&estimator);

    wheel_speed_estimator_update(&estimator, {65530U, 65534U}, 1000U);

    const auto result = wheel_speed_estimator_update(
        &estimator, {4U, 2U}, 1100U);

    EXPECT_TRUE(result.valid);
    EXPECT_NEAR(result.left_mm_s, 21.52f, 0.1f);
    EXPECT_NEAR(result.right_mm_s, 8.61f, 0.1f);
}

TEST(WheelSpeedEstimatorTest, HandlesTimestampRollover)
{
    WheelSpeedEstimator estimator{};
    wheel_speed_estimator_init(&estimator);

    wheel_speed_estimator_update(
        &estimator, {100U, 100U}, UINT32_MAX - 49U);

    const auto result = wheel_speed_estimator_update(
        &estimator, {200U, 200U}, 50U);

    EXPECT_TRUE(result.valid);
    EXPECT_NEAR(result.left_mm_s, 215.2f, 0.1f);
    EXPECT_NEAR(result.right_mm_s, 215.2f, 0.1f);
}

TEST(WheelSpeedEstimatorTest, NoPulsesProducesValidZeroSpeed)
{
    WheelSpeedEstimator estimator{};
    wheel_speed_estimator_init(&estimator);

    wheel_speed_estimator_update(&estimator, {100U, 200U}, 1000U);

    const auto result = wheel_speed_estimator_update(
        &estimator, {100U, 200U}, 1100U);

    EXPECT_TRUE(result.valid);
    EXPECT_FLOAT_EQ(result.left_mm_s, 0.0f);
    EXPECT_FLOAT_EQ(result.right_mm_s, 0.0f);
}