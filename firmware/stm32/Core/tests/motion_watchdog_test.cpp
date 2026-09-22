#include <gtest/gtest.h>

extern "C"
{
#include "motion_watchdog.h"
}

TEST(MotionWatchdogTest, RejectsZeroTimeoutAndRemainsInactive)
{
    EXPECT_FALSE(motion_watchdog_init(0U));
    EXPECT_FALSE(motion_watchdog_refresh(100U));
    EXPECT_FALSE(motion_watchdog_is_armed());
    EXPECT_FALSE(motion_watchdog_expired(1000U));
}

TEST(MotionWatchdogTest, RefreshArmsInitializedWatchdog)
{
    ASSERT_TRUE(motion_watchdog_init(250U));

    EXPECT_FALSE(motion_watchdog_is_armed());
    EXPECT_FALSE(motion_watchdog_expired(1000U));

    ASSERT_TRUE(motion_watchdog_refresh(1000U));

    EXPECT_TRUE(motion_watchdog_is_armed());
    EXPECT_FALSE(motion_watchdog_expired(1000U));
}

TEST(MotionWatchdogTest, ExpiresAtTimeoutBoundary)
{
    ASSERT_TRUE(motion_watchdog_init(250U));
    ASSERT_TRUE(motion_watchdog_refresh(1000U));

    EXPECT_FALSE(motion_watchdog_expired(1249U));
    EXPECT_TRUE(motion_watchdog_expired(1250U));
    EXPECT_TRUE(motion_watchdog_expired(1251U));
}

TEST(MotionWatchdogTest, RefreshRestartsTimeout)
{
    ASSERT_TRUE(motion_watchdog_init(250U));
    ASSERT_TRUE(motion_watchdog_refresh(1000U));

    EXPECT_FALSE(motion_watchdog_expired(1200U));

    ASSERT_TRUE(motion_watchdog_refresh(1200U));

    EXPECT_FALSE(motion_watchdog_expired(1449U));
    EXPECT_TRUE(motion_watchdog_expired(1450U));
}

TEST(MotionWatchdogTest, DisarmSuppressesExpiration)
{
    ASSERT_TRUE(motion_watchdog_init(250U));
    ASSERT_TRUE(motion_watchdog_refresh(1000U));
    ASSERT_TRUE(motion_watchdog_is_armed());

    motion_watchdog_disarm();

    EXPECT_FALSE(motion_watchdog_is_armed());
    EXPECT_FALSE(motion_watchdog_expired(5000U));
}

TEST(MotionWatchdogTest, InvalidReinitializationClearsActiveState)
{
    ASSERT_TRUE(motion_watchdog_init(250U));
    ASSERT_TRUE(motion_watchdog_refresh(1000U));
    ASSERT_TRUE(motion_watchdog_is_armed());

    EXPECT_FALSE(motion_watchdog_init(0U));

    EXPECT_FALSE(motion_watchdog_is_armed());
    EXPECT_FALSE(motion_watchdog_refresh(1100U));
    EXPECT_FALSE(motion_watchdog_expired(5000U));
}

TEST(MotionWatchdogTest, ReinitializationDisarmsAndReplacesTimeout)
{
    ASSERT_TRUE(motion_watchdog_init(250U));
    ASSERT_TRUE(motion_watchdog_refresh(1000U));

    ASSERT_TRUE(motion_watchdog_init(100U));

    EXPECT_FALSE(motion_watchdog_is_armed());

    ASSERT_TRUE(motion_watchdog_refresh(2000U));

    EXPECT_FALSE(motion_watchdog_expired(2099U));
    EXPECT_TRUE(motion_watchdog_expired(2100U));
}

TEST(MotionWatchdogTest, ExpiresAcrossTickCounterWraparound)
{
    constexpr uint32_t timeout_ms = 250U;
    constexpr uint32_t refreshed_at_ms = UINT32_MAX - 99U;

    ASSERT_TRUE(motion_watchdog_init(timeout_ms));
    ASSERT_TRUE(motion_watchdog_refresh(refreshed_at_ms));

    EXPECT_FALSE(motion_watchdog_expired(149U));
    EXPECT_TRUE(motion_watchdog_expired(150U));
}