#include <gtest/gtest.h>

#include "manual_control_config.h"
#include "manual_input_guard.h"

TEST(ManualInputGuardTest, StartupWithEnableHighDoesNotArm)
{
    ManualInputGuard guard;
    const auto now = ManualInputGuard::Clock::time_point{};

    CrsfChannels channels{};
    channels.values[manual_control_config::steering_channel] =
        manual_control_config::stick_center;
    channels.values[manual_control_config::forward_channel] =
        manual_control_config::stick_center;
    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_high_min;

    EXPECT_FALSE(guard.poll(now));

    guard.on_channels(channels, now);
    EXPECT_FALSE(guard.poll(now));

    guard.on_channels(channels, now + std::chrono::milliseconds{50});
    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{50}));
}

TEST(ManualInputGuardTest, ArmsAfterNeutralDisabledThenEnabledAndExpiresAt200Ms)
{
    ManualInputGuard guard;
    const auto now = ManualInputGuard::Clock::time_point{};

    CrsfChannels channels{};
    channels.values[manual_control_config::steering_channel] =
        manual_control_config::stick_center;
    channels.values[manual_control_config::forward_channel] =
        manual_control_config::stick_center;
    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_low_max;

    EXPECT_FALSE(guard.poll(now));
    guard.on_channels(channels, now);
    EXPECT_FALSE(guard.poll(now));

    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_high_min;

    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{10}));
    guard.on_channels(channels, now + std::chrono::milliseconds{10});
    EXPECT_TRUE(guard.poll(now + std::chrono::milliseconds{10}));

    EXPECT_TRUE(guard.poll(now + std::chrono::milliseconds{209}));
    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{210}));
}

TEST(ManualInputGuardTest, ReconnectWithEnableHighRequiresRearming)
{
    ManualInputGuard guard;
    const auto now = ManualInputGuard::Clock::time_point{};

    CrsfChannels channels{};
    channels.values[manual_control_config::steering_channel] =
        manual_control_config::stick_center;
    channels.values[manual_control_config::forward_channel] =
        manual_control_config::stick_center;
    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_low_max;

    EXPECT_FALSE(guard.poll(now));
    guard.on_channels(channels, now);
    EXPECT_FALSE(guard.poll(now));

    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_high_min;
    guard.on_channels(channels, now + std::chrono::milliseconds{10});
    EXPECT_TRUE(guard.poll(now+std::chrono::milliseconds{10}));

    guard.on_channels(channels, now + std::chrono::milliseconds{510});
    EXPECT_FALSE(guard.poll(now+std::chrono::milliseconds{510}));

    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_low_max;
    guard.on_channels(channels, now + std::chrono::milliseconds{520});
    EXPECT_FALSE(guard.poll(now+std::chrono::milliseconds{520}));

    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_high_min;
    guard.on_channels(channels, now + std::chrono::milliseconds{530});
    EXPECT_TRUE(guard.poll(now+std::chrono::milliseconds{530}));

}

TEST(ManualInputGuardTest, EnableWithDisplacedStickRequiresNewEnableCycle)
{
    ManualInputGuard guard;
    const auto now = ManualInputGuard::Clock::time_point{};

    CrsfChannels channels{};
    channels.values[manual_control_config::steering_channel] =
        manual_control_config::stick_center;
    channels.values[manual_control_config::forward_channel] =
        manual_control_config::stick_center;
    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_low_max;

    EXPECT_FALSE(guard.poll(now));
    guard.on_channels(channels, now);
    EXPECT_FALSE(guard.poll(now));

    channels.values[manual_control_config::steering_channel] =
        manual_control_config::stick_center + manual_control_config::neutral_tolerance + 1U;
    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_high_min;

    guard.on_channels(channels, now + std::chrono::milliseconds{10});
    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{10}));

    channels.values[manual_control_config::steering_channel] =
        manual_control_config::stick_center;

    guard.on_channels(channels, now + std::chrono::milliseconds{20});
    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{20}));

    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_low_max;
    guard.on_channels(channels, now + std::chrono::milliseconds{30});
    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{30}));

    channels.values[manual_control_config::enable_channel] =
        manual_control_config::enable_high_min;
    guard.on_channels(channels, now + std::chrono::milliseconds{40});
    EXPECT_TRUE(guard.poll(now + std::chrono::milliseconds{40}));

}

TEST(ManualInputGuardTest, InvalidEnableValueRequiresRearming)
{
    ManualInputGuard guard;
    const auto now = ManualInputGuard::Clock::time_point{};

    CrsfChannels channels{};
    channels.values[manual_control_config::steering_channel] = manual_control_config::stick_center;
    channels.values[manual_control_config::forward_channel] = manual_control_config::stick_center;
    channels.values[manual_control_config::enable_channel] = manual_control_config::enable_low_max;

    EXPECT_FALSE(guard.poll(now));
    guard.on_channels(channels, now);
    EXPECT_FALSE(guard.poll(now));

    channels.values[manual_control_config::enable_channel] = manual_control_config::enable_high_min;
    guard.on_channels(channels, now + std::chrono::milliseconds{10});
    EXPECT_TRUE(guard.poll(now + std::chrono::milliseconds{10}));

    channels.values[manual_control_config::enable_channel] = 1000U;
    guard.on_channels(channels, now + std::chrono::milliseconds{20});
    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{20}));

    channels.values[manual_control_config::enable_channel] = manual_control_config::enable_high_min;
    guard.on_channels(channels, now + std::chrono::milliseconds{30});
    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{30}));

    channels.values[manual_control_config::enable_channel] = manual_control_config::enable_low_max;
    guard.on_channels(channels, now + std::chrono::milliseconds{40});
    EXPECT_FALSE(guard.poll(now + std::chrono::milliseconds{40}));

    channels.values[manual_control_config::enable_channel] = manual_control_config::enable_high_min;
    guard.on_channels(channels, now + std::chrono::milliseconds{50});
    EXPECT_TRUE(guard.poll(now + std::chrono::milliseconds{50}));

}