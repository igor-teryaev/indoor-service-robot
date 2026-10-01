#include <gtest/gtest.h>

extern "C"
{
#include "motor_driver.h"
#include "motor_driver_dri0041_port.h"
}

namespace
{
    uint32_t port_now_ms = 0U;

    bool port_init_result = true;
    uint32_t port_init_count = 0U;

    bool port_apply_result = true;
    uint32_t port_apply_count = 0U;

    Dri0041PortControl port_control = {};
}

extern "C" bool motor_driver_dri0041_port_init(void)
{
    ++port_init_count;
    return port_init_result;
}

extern "C" bool motor_driver_dri0041_port_apply(
    const Dri0041PortControl *control)
{
    ++port_apply_count;
    port_control = *control;
    return port_apply_result;
}

extern "C" uint32_t motor_driver_dri0041_port_now_ms(void)
{
    return port_now_ms;
}

class Dri0041MotorDriverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        port_init_result = false;
        (void)motor_driver_init();

        port_init_result = true;
        port_apply_result = true;

        port_init_count = 0U;
        port_apply_count = 0U;
        port_now_ms = 0U;
        port_control = {};
    }
};

TEST_F(
    Dri0041MotorDriverTest,
    AppliesMixedDirectionCommandAfterInitialization)
{
    ASSERT_TRUE(motor_driver_init());

    const MotorDriverCommand command =
    {
        .left = 250,
        .right = -750
    };

    ASSERT_TRUE(motor_driver_apply(&command));

    EXPECT_EQ(port_init_count, 1U);
    ASSERT_EQ(port_apply_count, 1U);

    EXPECT_EQ(port_control.left_duty, 250U);
    EXPECT_TRUE(port_control.left_in1);
    EXPECT_FALSE(port_control.left_in2);

    EXPECT_EQ(port_control.right_duty, 750U);
    EXPECT_FALSE(port_control.right_in3);
    EXPECT_TRUE(port_control.right_in4);
}

TEST_F(
    Dri0041MotorDriverTest,
    StopAppliesBrakeToBothChannels)
{
    ASSERT_TRUE(motor_driver_init());
    ASSERT_TRUE(motor_driver_stop());

    ASSERT_EQ(port_apply_count, 1U);

    EXPECT_EQ(port_control.left_duty, 0U);
    EXPECT_FALSE(port_control.left_in1);
    EXPECT_FALSE(port_control.left_in2);

    EXPECT_EQ(port_control.right_duty, 0U);
    EXPECT_FALSE(port_control.right_in3);
    EXPECT_FALSE(port_control.right_in4);
}

TEST_F(
    Dri0041MotorDriverTest,
    ZeroCommandAppliesBrakeToBothChannels)
{
    ASSERT_TRUE(motor_driver_init());

    constexpr MotorDriverCommand command =
    {
        .left = 0,
        .right = 0
    };

    ASSERT_TRUE(motor_driver_apply(&command));

    ASSERT_EQ(port_apply_count, 1U);

    EXPECT_EQ(port_control.left_duty, 0U);
    EXPECT_FALSE(port_control.left_in1);
    EXPECT_FALSE(port_control.left_in2);

    EXPECT_EQ(port_control.right_duty, 0U);
    EXPECT_FALSE(port_control.right_in3);
    EXPECT_FALSE(port_control.right_in4);
}

TEST_F(
    Dri0041MotorDriverTest,
    RejectsOutOfRangeCommandsWithoutApplying)
{
    ASSERT_TRUE(motor_driver_init());

    const MotorDriverCommand invalid_left =
    {
        .left = MOTOR_DRIVER_COMMAND_MAX + 1,
        .right = 100
    };

    const MotorDriverCommand invalid_right =
    {
        .left = -100,
        .right = MOTOR_DRIVER_COMMAND_MIN - 1
    };

    EXPECT_FALSE(motor_driver_apply(&invalid_left));
    EXPECT_FALSE(motor_driver_apply(&invalid_right));

    EXPECT_EQ(port_apply_count, 0U);
}

TEST_F(
    Dri0041MotorDriverTest,
    FailedReinitializationDisablesDriver)
{
    port_init_result = true;
    ASSERT_TRUE(motor_driver_init());

    port_init_result = false;
    ASSERT_FALSE(motor_driver_init());

    ASSERT_FALSE(motor_driver_stop());
    EXPECT_EQ(port_apply_count, 0U);
}

TEST_F(
    Dri0041MotorDriverTest,
    PropagatesPortApplyFailures)
{
    ASSERT_TRUE(motor_driver_init());

    port_apply_result = false;

    constexpr MotorDriverCommand command =
    {
        .left = 100,
        .right = -500
    };

    EXPECT_FALSE(motor_driver_apply(&command));
    EXPECT_FALSE(motor_driver_stop());

    EXPECT_EQ(port_apply_count, 2U);
}

TEST_F(
    Dri0041MotorDriverTest,
    ApplyBeforeInitDoesNotAccessPort)
{
    const MotorDriverCommand command =
    {
        .left = 100,
        .right = -100
    };

    EXPECT_FALSE(motor_driver_apply(&command));
    EXPECT_EQ(port_apply_count, 0U);
}

TEST_F(
    Dri0041MotorDriverTest,
    StopBeforeInitDoesNotAccessPort)
{
    EXPECT_FALSE(motor_driver_stop());
    EXPECT_EQ(port_apply_count, 0U);
}

TEST_F(
    Dri0041MotorDriverTest,
    AcceptsInclusiveCommandLimits)
{
    ASSERT_TRUE(motor_driver_init());

    constexpr MotorDriverCommand command =
    {
        .left = MOTOR_DRIVER_COMMAND_MAX,
        .right = MOTOR_DRIVER_COMMAND_MIN
    };

    ASSERT_TRUE(motor_driver_apply(&command));

    ASSERT_EQ(port_apply_count, 1U);

    EXPECT_EQ(port_control.left_duty, 1000U);
    EXPECT_TRUE(port_control.left_in1);
    EXPECT_FALSE(port_control.left_in2);

    EXPECT_EQ(port_control.right_duty, 1000U);
    EXPECT_FALSE(port_control.right_in3);
    EXPECT_TRUE(port_control.right_in4);
}

TEST_F(
    Dri0041MotorDriverTest,
    BrakesBeforeReversingLeftMotor)
{
    ASSERT_TRUE(motor_driver_init());

    constexpr MotorDriverCommand forward =
    {
        .left = 400,
        .right = 200
    };

    ASSERT_TRUE(motor_driver_apply(&forward));

    constexpr MotorDriverCommand reversed =
    {
        .left = -400,
        .right = 200
    };

    ASSERT_TRUE(motor_driver_apply(&reversed));

    ASSERT_EQ(port_apply_count, 2U);

    EXPECT_EQ(port_control.left_duty, 0U);
    EXPECT_FALSE(port_control.left_in1);
    EXPECT_FALSE(port_control.left_in2);

    EXPECT_EQ(port_control.right_duty, 200U);
    EXPECT_TRUE(port_control.right_in3);
    EXPECT_FALSE(port_control.right_in4);
}

TEST_F(
    Dri0041MotorDriverTest,
    FailedBrakeDuringReversalDoesNotAdvanceDirectionState)
{
    ASSERT_TRUE(motor_driver_init());

    constexpr MotorDriverCommand forward =
    {
        .left = 400,
        .right = 0
    };

    ASSERT_TRUE(motor_driver_apply(&forward));

    constexpr MotorDriverCommand reverse =
    {
        .left = -400,
        .right = 0
    };

    port_apply_result = false;

    EXPECT_FALSE(motor_driver_apply(&reverse));

    port_apply_result = true;

    ASSERT_TRUE(motor_driver_apply(&reverse));

    ASSERT_EQ(port_apply_count, 3U);

    EXPECT_EQ(port_control.left_duty, 0U);
    EXPECT_FALSE(port_control.left_in1);
    EXPECT_FALSE(port_control.left_in2);
}

TEST_F(
    Dri0041MotorDriverTest,
    KeepsBrakingUntilReversalDelayExpires)
{
    ASSERT_TRUE(motor_driver_init());

    constexpr MotorDriverCommand forward =
    {
        .left = 400,
        .right = 0
    };

    ASSERT_TRUE(motor_driver_apply(&forward));

    constexpr MotorDriverCommand reverse =
    {
        .left = -400,
        .right = 0
    };

    port_now_ms = 10U;

    ASSERT_TRUE(motor_driver_apply(&reverse));

    EXPECT_EQ(port_control.left_duty, 0U);
    EXPECT_FALSE(port_control.left_in1);
    EXPECT_FALSE(port_control.left_in2);

    port_now_ms = 109U;

    ASSERT_TRUE(motor_driver_apply(&reverse));

    EXPECT_EQ(port_control.left_duty, 0U);
    EXPECT_FALSE(port_control.left_in1);
    EXPECT_FALSE(port_control.left_in2);

    port_now_ms = 110U;

    ASSERT_TRUE(motor_driver_apply(&reverse));

    EXPECT_EQ(port_control.left_duty, 400U);
    EXPECT_FALSE(port_control.left_in1);
    EXPECT_TRUE(port_control.left_in2);
}