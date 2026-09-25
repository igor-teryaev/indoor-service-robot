#include <gtest/gtest.h>

extern "C"
{
#include "motor_driver.h"
#include "motor_driver_port.h"
}

namespace
{
    bool motor_driver_port_init_result = false;
    uint32_t motor_driver_port_init_count = 0U;

    bool motor_driver_port_apply_result = true;
    uint32_t motor_driver_port_apply_count = 0U;

    MotorDriverPortControl motor_driver_port_control = {0};
}


extern "C" bool motor_driver_port_init(void)
{
    motor_driver_port_init_count++;
    return motor_driver_port_init_result;
}

extern "C" bool motor_driver_port_apply(const MotorDriverPortControl *control)
{
    ++motor_driver_port_apply_count;
    motor_driver_port_control = *control;
    return motor_driver_port_apply_result;
}

class MotorDriverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        motor_driver_port_init_result = false;
        (void)motor_driver_init();

        motor_driver_port_init_result = true;
        motor_driver_port_apply_result = true;

        motor_driver_port_init_count = 0U;
        motor_driver_port_apply_count = 0U;
        motor_driver_port_control = {};
    }
};

TEST_F(MotorDriverTest, ApplyBeforeInitDoesNotAccessPort)
{
    const MotorDriverCommand command = {100, -100};

    EXPECT_FALSE(motor_driver_apply(&command));
    EXPECT_EQ(motor_driver_port_apply_count, 0U);
}

TEST_F(MotorDriverTest, AppliesMixedDirectionCommandAfterInitialization)
{
    ASSERT_TRUE(motor_driver_init());

    const MotorDriverCommand command = {250, -750};
    ASSERT_TRUE(motor_driver_apply(&command));

    EXPECT_EQ(motor_driver_port_init_count, 1U);
    ASSERT_EQ(motor_driver_port_apply_count, 1U);

    EXPECT_EQ(motor_driver_port_control.ain1_duty, 250U);
    EXPECT_EQ(motor_driver_port_control.ain2_duty, 0U);
    EXPECT_EQ(motor_driver_port_control.bin1_duty, 0U);
    EXPECT_EQ(motor_driver_port_control.bin2_duty, 750U);
}

TEST_F(MotorDriverTest, RejectsOutOfRangeCommandsWithoutApplying)
{
    ASSERT_TRUE(motor_driver_init());

    const MotorDriverCommand invalid_left =
    {
        MOTOR_DRIVER_COMMAND_MAX + 1,
        100
    };

    const MotorDriverCommand invalid_right =
    {
        -100,
        MOTOR_DRIVER_COMMAND_MIN - 1
    };

    EXPECT_FALSE(motor_driver_apply(&invalid_left));
    EXPECT_FALSE(motor_driver_apply(&invalid_right));

    EXPECT_EQ(motor_driver_port_apply_count, 0U);
}

TEST_F(MotorDriverTest, StopBeforeInitDoesNotAccessPort)
{
    EXPECT_FALSE(motor_driver_stop());
    EXPECT_EQ(motor_driver_port_apply_count, 0U);
}

TEST_F(MotorDriverTest, StopAppliesBrakeToBothChannels)
{
    ASSERT_TRUE(motor_driver_init());
    ASSERT_TRUE(motor_driver_stop());
    EXPECT_EQ(motor_driver_port_apply_count, 1U);
    EXPECT_EQ(motor_driver_port_control.ain1_duty, MOTOR_DRIVER_COMMAND_MAX);
    EXPECT_EQ(motor_driver_port_control.ain2_duty, MOTOR_DRIVER_COMMAND_MAX);
    EXPECT_EQ(motor_driver_port_control.bin1_duty, MOTOR_DRIVER_COMMAND_MAX);
    EXPECT_EQ(motor_driver_port_control.bin2_duty, MOTOR_DRIVER_COMMAND_MAX);
}

TEST_F(MotorDriverTest, ZeroCommandAppliesCoastToBothChannels)
{
    ASSERT_TRUE(motor_driver_init());
    constexpr MotorDriverCommand command = {.left = 0, .right = 0};
    ASSERT_TRUE(motor_driver_apply(&command));
    EXPECT_EQ(motor_driver_port_apply_count, 1U);
    EXPECT_EQ(motor_driver_port_control.ain1_duty, 0U);
    EXPECT_EQ(motor_driver_port_control.ain2_duty, 0U);
    EXPECT_EQ(motor_driver_port_control.bin1_duty, 0U);
    EXPECT_EQ(motor_driver_port_control.bin2_duty, 0U);
}

TEST_F(MotorDriverTest, FailedReinitializationDisablesDriver)
{
    motor_driver_port_init_result = true;
    ASSERT_TRUE(motor_driver_init());
    motor_driver_port_init_result = false;
    ASSERT_FALSE(motor_driver_init());
    ASSERT_FALSE(motor_driver_stop());
    EXPECT_EQ(motor_driver_port_apply_count, 0U);
}

TEST_F(MotorDriverTest, PropagatesPortApplyFailures)
{
    ASSERT_TRUE(motor_driver_init());
    motor_driver_port_apply_result = false;
    constexpr MotorDriverCommand command = {.left = 10, .right = -500};
    ASSERT_FALSE(motor_driver_apply(&command));
    ASSERT_FALSE(motor_driver_stop());
    EXPECT_EQ(motor_driver_port_apply_count, 2U);
}

TEST_F(MotorDriverTest, AcceptsInclusiveCommandLimits)
{
    ASSERT_TRUE(motor_driver_init());
    constexpr MotorDriverCommand command = {.left = MOTOR_DRIVER_COMMAND_MAX, .right = MOTOR_DRIVER_COMMAND_MIN};
    ASSERT_TRUE(motor_driver_apply(&command));
    EXPECT_EQ(motor_driver_port_apply_count, 1U);
    EXPECT_EQ(motor_driver_port_control.ain1_duty, 1000U);
    EXPECT_EQ(motor_driver_port_control.ain2_duty, 0U);
    EXPECT_EQ(motor_driver_port_control.bin1_duty, 0U);
    EXPECT_EQ(motor_driver_port_control.bin2_duty, 1000U);
}