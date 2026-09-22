#include <gtest/gtest.h>

extern "C"
{
#include "motion_command_guard.h"
}

namespace
{
    bool motor_init_result = true;
    bool motor_apply_result = true;
    bool motor_stop_result = true;

    uint32_t motor_init_count = 0U;
    uint32_t motor_apply_count = 0U;
    uint32_t motor_stop_count = 0U;

    MotorDriverCommand last_command = {0};
}

extern "C" bool motor_driver_init(void)
{
    ++motor_init_count;
    return motor_init_result;
}

extern "C" bool motor_driver_apply(
    const MotorDriverCommand *command)
{
    ++motor_apply_count;

    if (command != nullptr)
    {
        last_command = *command;
    }

    return motor_apply_result;
}

extern "C" bool motor_driver_stop(void)
{
    ++motor_stop_count;
    return motor_stop_result;
}

class MotionCommandGuardTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        motor_init_result = true;
        motor_apply_result = true;
        motor_stop_result = true;

        motor_init_count = 0U;
        motor_apply_count = 0U;
        motor_stop_count = 0U;
        last_command = {0};

        // Invalid init clears the static guard and watchdog state.
        ASSERT_FALSE(motion_command_guard_init(0U));

        motor_init_count = 0U;
        motor_apply_count = 0U;
        motor_stop_count = 0U;
    }
};

TEST_F(MotionCommandGuardTest, RejectsUseBeforeInitialization)
{
    constexpr MotorDriverCommand command =
    {
        .left = 500,
        .right = 500
    };

    EXPECT_FALSE(motion_command_guard_apply(&command, 100U));
    EXPECT_FALSE(motion_command_guard_stop());

    EXPECT_EQ(
        motion_command_guard_update(1000U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);

    EXPECT_EQ(motor_apply_count, 0U);
    EXPECT_EQ(motor_stop_count, 0U);
}

TEST_F(MotionCommandGuardTest, StopsOnceAtCommandTimeout)
{
    ASSERT_TRUE(motion_command_guard_init(250U));
    EXPECT_EQ(motor_init_count, 1U);

    constexpr MotorDriverCommand command =
    {
        .left = 600,
        .right = -700
    };

    ASSERT_TRUE(motion_command_guard_apply(&command, 1000U));

    ASSERT_EQ(motor_apply_count, 1U);
    EXPECT_EQ(last_command.left, 600);
    EXPECT_EQ(last_command.right, -700);

    EXPECT_EQ(
        motion_command_guard_update(1249U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);
    EXPECT_EQ(motor_stop_count, 0U);

    EXPECT_EQ(
        motion_command_guard_update(1250U),
        MOTION_COMMAND_GUARD_UPDATE_STOPPED);
    EXPECT_EQ(motor_stop_count, 1U);

    EXPECT_EQ(
        motion_command_guard_update(1251U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);
    EXPECT_EQ(motor_stop_count, 1U);
}

TEST_F(MotionCommandGuardTest, ApplyFailureImmediatelyStopsPreviousMotion)
{
    ASSERT_TRUE(motion_command_guard_init(250U));

    constexpr MotorDriverCommand first_command =
    {
        .left = 600,
        .right = 600
    };

    constexpr MotorDriverCommand failed_command =
    {
        .left = -600,
        .right = -600
    };

    ASSERT_TRUE(
        motion_command_guard_apply(&first_command, 1000U));

    motor_apply_result = false;

    EXPECT_FALSE(
        motion_command_guard_apply(&failed_command, 1100U));

    EXPECT_EQ(motor_apply_count, 2U);
    EXPECT_EQ(motor_stop_count, 1U);

    EXPECT_EQ(
        motion_command_guard_update(5000U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);

    EXPECT_EQ(motor_stop_count, 1U);
}

TEST_F(MotionCommandGuardTest, StopFailureBlocksCommandsAndRetries)
{
    ASSERT_TRUE(motion_command_guard_init(250U));

    constexpr MotorDriverCommand command =
    {
        .left = 600,
        .right = 600
    };

    ASSERT_TRUE(
        motion_command_guard_apply(&command, 1000U));

    motor_stop_result = false;

    EXPECT_EQ(
        motion_command_guard_update(1250U),
        MOTION_COMMAND_GUARD_UPDATE_STOP_FAILED);

    EXPECT_EQ(motor_stop_count, 1U);

    EXPECT_FALSE(
        motion_command_guard_apply(&command, 1300U));

    EXPECT_EQ(motor_apply_count, 1U);

    motor_stop_result = true;

    EXPECT_EQ(
        motion_command_guard_update(1300U),
        MOTION_COMMAND_GUARD_UPDATE_STOPPED);

    EXPECT_EQ(motor_stop_count, 2U);

    EXPECT_TRUE(
        motion_command_guard_apply(&command, 1400U));

    EXPECT_EQ(motor_apply_count, 2U);
}

TEST_F(MotionCommandGuardTest, ZeroCommandDisarmsWatchdog)
{
    ASSERT_TRUE(motion_command_guard_init(250U));

    constexpr MotorDriverCommand move_command =
    {
        .left = 600,
        .right = 600
    };

    constexpr MotorDriverCommand coast_command =
    {
        .left = 0,
        .right = 0
    };

    ASSERT_TRUE(
        motion_command_guard_apply(&move_command, 1000U));

    ASSERT_TRUE(
        motion_command_guard_apply(&coast_command, 1100U));

    EXPECT_EQ(motor_apply_count, 2U);
    EXPECT_EQ(last_command.left, 0);
    EXPECT_EQ(last_command.right, 0);
    EXPECT_EQ(motor_stop_count, 0U);

    EXPECT_EQ(
        motion_command_guard_update(5000U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);

    EXPECT_EQ(motor_stop_count, 0U);
}

TEST_F(MotionCommandGuardTest, FreshCommandRestartsTimeout)
{
    ASSERT_TRUE(motion_command_guard_init(250U));

    constexpr MotorDriverCommand first_command =
    {
        .left = 600,
        .right = 600
    };

    constexpr MotorDriverCommand second_command =
    {
        .left = 700,
        .right = 500
    };

    ASSERT_TRUE(
        motion_command_guard_apply(&first_command, 1000U));

    ASSERT_TRUE(
        motion_command_guard_apply(&second_command, 1200U));

    EXPECT_EQ(
        motion_command_guard_update(1250U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);

    EXPECT_EQ(
        motion_command_guard_update(1449U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);

    EXPECT_EQ(
        motion_command_guard_update(1450U),
        MOTION_COMMAND_GUARD_UPDATE_STOPPED);

    EXPECT_EQ(motor_apply_count, 2U);
    EXPECT_EQ(motor_stop_count, 1U);
}

TEST_F(MotionCommandGuardTest, MotorInitializationFailureLeavesGuardInactive)
{
    motor_init_result = false;

    EXPECT_FALSE(motion_command_guard_init(250U));
    EXPECT_EQ(motor_init_count, 1U);

    constexpr MotorDriverCommand command =
    {
        .left = 600,
        .right = 600
    };

    EXPECT_FALSE(
        motion_command_guard_apply(&command, 1000U));

    EXPECT_FALSE(motion_command_guard_stop());

    EXPECT_EQ(
        motion_command_guard_update(5000U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);

    EXPECT_EQ(motor_apply_count, 0U);
    EXPECT_EQ(motor_stop_count, 0U);
}

TEST_F(MotionCommandGuardTest, ManualStopDisarmsUntilFreshCommand)
{
    ASSERT_TRUE(motion_command_guard_init(250U));

    constexpr MotorDriverCommand command =
    {
        .left = 600,
        .right = 600
    };

    ASSERT_TRUE(
        motion_command_guard_apply(&command, 1000U));

    ASSERT_TRUE(motion_command_guard_stop());

    EXPECT_EQ(motor_stop_count, 1U);

    EXPECT_EQ(
        motion_command_guard_update(5000U),
        MOTION_COMMAND_GUARD_UPDATE_NONE);

    EXPECT_EQ(motor_stop_count, 1U);

    ASSERT_TRUE(
        motion_command_guard_apply(&command, 5100U));

    EXPECT_EQ(motor_apply_count, 2U);
}