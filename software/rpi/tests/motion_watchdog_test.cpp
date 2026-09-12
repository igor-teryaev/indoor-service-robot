#include <gtest/gtest.h>
#include "motion_watchdog.h"
#include "motion_config.h"

TEST(MotionWatchdogTest, StartsDisarmedAndNotExpired)
{
    const MotionWatchdog watchdog{MOTION_TIMEOUT};

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    EXPECT_FALSE(watchdog.armed());
    EXPECT_FALSE(watchdog.expired(t0));
}

TEST(MotionWatchdogTest, RefreshArmsWatchdog)
{
    MotionWatchdog watchdog{MOTION_TIMEOUT};

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    watchdog.refresh(t0);

    EXPECT_TRUE(watchdog.armed());
    EXPECT_FALSE(watchdog.expired(t0));
}

TEST(MotionWatchdogTest, ExpiresAtTimeoutBoundary)
{
    MotionWatchdog watchdog{MOTION_TIMEOUT};

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    watchdog.refresh(t0);

    EXPECT_FALSE(
        watchdog.expired(
            t0 + MOTION_TIMEOUT - std::chrono::milliseconds{1}
        )
    );

    EXPECT_TRUE(
        watchdog.expired(
            t0 + MOTION_TIMEOUT
        )
    );
}

TEST(MotionWatchdogTest, RefreshRestartsTimeout)
{
    MotionWatchdog watchdog{MOTION_TIMEOUT};

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    watchdog.refresh(t0);

    const auto t1 =
        t0 + std::chrono::milliseconds{1000};

    watchdog.refresh(t1);

    EXPECT_FALSE(
        watchdog.expired(
            t1 + MOTION_TIMEOUT - std::chrono::milliseconds{1}
        )
    );

    EXPECT_TRUE(
        watchdog.expired(
            t1 + MOTION_TIMEOUT
        )
    );
}

TEST(MotionWatchdogTest, DisarmStopsExpirationTracking)
{
    MotionWatchdog watchdog{MOTION_TIMEOUT};

    const auto t0 =
        MotionWatchdog::Clock::time_point{};

    watchdog.refresh(t0);

    ASSERT_TRUE(watchdog.armed());

    watchdog.disarm();

    EXPECT_FALSE(watchdog.armed());

    EXPECT_FALSE(
        watchdog.expired(
            t0 + MOTION_TIMEOUT + std::chrono::seconds{10}
        )
    );
}