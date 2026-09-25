#include "motor_driver.h"
#include "motor_driver_port.h"

#include <stddef.h>

typedef struct
{
    uint16_t in1_duty;
    uint16_t in2_duty;
} Drv8833ChannelControl;

static bool initialized;

static bool drv8833_channel_control(
    int16_t command,
    Drv8833ChannelControl *control)
{
    if ((control == NULL) ||
        (command > MOTOR_DRIVER_COMMAND_MAX) ||
        (command < MOTOR_DRIVER_COMMAND_MIN))
    {
        return false;
    }

    Drv8833ChannelControl m_control = {0};

    if (command > 0)
    {
        m_control.in1_duty = (uint16_t)command;
    }
    else if (command < 0)
    {
        m_control.in2_duty = (uint16_t)(-command);
    }

    *control = m_control;

    return true;
}

static bool drv8833_control_from_command(
    const MotorDriverCommand *command,
    MotorDriverPortControl *control)
{
    if ((command == NULL) || (control == NULL))
    {
        return false;
    }

    Drv8833ChannelControl channel_a = {0};
    Drv8833ChannelControl channel_b = {0};

    if (!drv8833_channel_control(
            command->left,
            &channel_a))
    {
        return false;
    }

    if (!drv8833_channel_control(
            command->right,
            &channel_b))
    {
        return false;
    }

    const MotorDriverPortControl new_control =
    {
        .ain1_duty = channel_a.in1_duty,
        .ain2_duty = channel_a.in2_duty,
        .bin1_duty = channel_b.in1_duty,
        .bin2_duty = channel_b.in2_duty
    };

    *control = new_control;

    return true;
}

static MotorDriverPortControl drv8833_brake_control(void)
{
    return (MotorDriverPortControl)
    {
        .ain1_duty = MOTOR_DRIVER_COMMAND_MAX,
        .ain2_duty = MOTOR_DRIVER_COMMAND_MAX,
        .bin1_duty = MOTOR_DRIVER_COMMAND_MAX,
        .bin2_duty = MOTOR_DRIVER_COMMAND_MAX
    };
}

bool motor_driver_init(void)
{
    initialized = false;

    if (!motor_driver_port_init())
    {
        return false;
    }

    initialized = true;
    return true;
}

bool motor_driver_apply(const MotorDriverCommand *command)
{
    if (!initialized || command == NULL)
    {
        return false;
    }

    MotorDriverPortControl control = {0};
    if (!drv8833_control_from_command(command, &control))
    {
        return false;
    }

    if (!motor_driver_port_apply(&control))
    {
        return false;
    }
    return true;
}

bool motor_driver_stop(void)
{
    if (!initialized)
    {
        return false;
    }

    const MotorDriverPortControl control = drv8833_brake_control();
    if (!motor_driver_port_apply(&control))
    {
        return false;
    }
    return true;
}
