#include "motor_driver.h"
#include "motor_driver_dri0041_port.h"

#include <stddef.h>

static bool initialized;

typedef enum
{
    DRI0041_DIRECTION_STOPPED = 0,
    DRI0041_DIRECTION_FORWARD,
    DRI0041_DIRECTION_REVERSE
} Dri0041Direction;

static Dri0041Direction left_direction;
static Dri0041Direction right_direction;

#define DRI0041_REVERSAL_BRAKE_MS 100U

static bool left_reversal_pending;
static bool right_reversal_pending;

static Dri0041Direction left_reversal_direction;
static Dri0041Direction right_reversal_direction;

static uint32_t left_reversal_started_ms;
static uint32_t right_reversal_started_ms;

static Dri0041Direction direction_from_command(
    int16_t command)
{
    if (command > 0)
    {
        return DRI0041_DIRECTION_FORWARD;
    }

    if (command < 0)
    {
        return DRI0041_DIRECTION_REVERSE;
    }

    return DRI0041_DIRECTION_STOPPED;
}

static bool direction_reverses(
    Dri0041Direction current,
    Dri0041Direction requested)
{
    return
        (current != DRI0041_DIRECTION_STOPPED) &&
        (requested != DRI0041_DIRECTION_STOPPED) &&
        (current != requested);
}

static bool dri0041_control_from_command(
    const MotorDriverCommand *command,
    Dri0041PortControl *control)
{
    if ((command == NULL) ||
        (control == NULL) ||
        (command->left > MOTOR_DRIVER_COMMAND_MAX) ||
        (command->left < MOTOR_DRIVER_COMMAND_MIN) ||
        (command->right > MOTOR_DRIVER_COMMAND_MAX) ||
        (command->right < MOTOR_DRIVER_COMMAND_MIN))
    {
        return false;
    }

    Dri0041PortControl new_control = {0};

    if (command->left > 0)
    {
        new_control.left_duty = (uint16_t)command->left;
        new_control.left_in1 = true;
    }
    else if (command->left < 0)
    {
        new_control.left_duty = (uint16_t)(-command->left);
        new_control.left_in2 = true;
    }

    if (command->right > 0)
    {
        new_control.right_duty = (uint16_t)command->right;
        new_control.right_in3 = true;
    }
    else if (command->right < 0)
    {
        new_control.right_duty = (uint16_t)(-command->right);
        new_control.right_in4 = true;
    }

    *control = new_control;
    return true;
}

bool motor_driver_init(void)
{
    initialized = false;
    left_direction = DRI0041_DIRECTION_STOPPED;
    right_direction = DRI0041_DIRECTION_STOPPED;
    left_reversal_direction = DRI0041_DIRECTION_STOPPED;
    right_reversal_direction = DRI0041_DIRECTION_STOPPED;

    left_reversal_pending = false;
    right_reversal_pending = false;

    left_reversal_started_ms = 0U;
    right_reversal_started_ms = 0U;

    if (!motor_driver_dri0041_port_init())
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

    Dri0041PortControl control = {0};

    if (!dri0041_control_from_command(
            command,
            &control))
    {
        return false;
    }

    const Dri0041Direction requested_left = direction_from_command(command->left);
    const Dri0041Direction requested_right = direction_from_command(command->right);

    const uint32_t now_ms = motor_driver_dri0041_port_now_ms();

    bool start_left_reversal = false;
    bool start_right_reversal = false;

    if (left_reversal_pending)
    {
        if (requested_left == DRI0041_DIRECTION_STOPPED)
        {
            control.left_duty = 0U;
            control.left_in1 = false;
            control.left_in2 = false;
        }
        else if (requested_left == left_direction)
        {
            left_reversal_pending = false;
        }
        else if (requested_left == left_reversal_direction)
        {
            const uint32_t elapsed_ms =
                now_ms - left_reversal_started_ms;

            if (elapsed_ms < DRI0041_REVERSAL_BRAKE_MS)
            {
                control.left_duty = 0U;
                control.left_in1 = false;
                control.left_in2 = false;
            }
        }
    }
    else if (direction_reverses(
                 left_direction,
                 requested_left))
    {
        control.left_duty = 0U;
        control.left_in1 = false;
        control.left_in2 = false;

        start_left_reversal = true;
    }

    if (right_reversal_pending)
    {
        if (requested_right == DRI0041_DIRECTION_STOPPED)
        {
            control.right_duty = 0U;
            control.right_in3 = false;
            control.right_in4 = false;
        }
        else if (requested_right == right_direction)
        {
            right_reversal_pending = false;
        }
        else if (requested_right == right_reversal_direction)
        {
            const uint32_t elapsed_ms =
                now_ms - right_reversal_started_ms;

            if (elapsed_ms < DRI0041_REVERSAL_BRAKE_MS)
            {
                control.right_duty = 0U;
                control.right_in3 = false;
                control.right_in4 = false;
            }
        }
    }
    else if (direction_reverses(
                 right_direction,
                 requested_right))
    {
        control.right_duty = 0U;
        control.right_in3 = false;
        control.right_in4 = false;

        start_right_reversal = true;
    }

    if (!motor_driver_dri0041_port_apply(&control))
    {
        return false;
    }

    if (start_left_reversal)
    {
        left_reversal_pending = true;
        left_reversal_direction = requested_left;
        left_reversal_started_ms = now_ms;
    }
    else if (left_reversal_pending)
    {
        if (requested_left == DRI0041_DIRECTION_STOPPED)
        {
            left_reversal_pending = false;
            left_direction = DRI0041_DIRECTION_STOPPED;
        }
        else if (requested_left == left_direction)
        {
            left_reversal_pending = false;
        }
        else if ((requested_left == left_reversal_direction) &&
                 ((uint32_t)(
                      now_ms -
                      left_reversal_started_ms) >=
                  DRI0041_REVERSAL_BRAKE_MS))
        {
            left_reversal_pending = false;
            left_direction = requested_left;
        }
    }
    else
    {
        left_direction = requested_left;
    }

    if (start_right_reversal)
    {
        right_reversal_pending = true;
        right_reversal_direction = requested_right;
        right_reversal_started_ms = now_ms;
    }
    else if (right_reversal_pending)
    {
        if (requested_right == DRI0041_DIRECTION_STOPPED)
        {
            right_reversal_pending = false;
            right_direction = DRI0041_DIRECTION_STOPPED;
        }
        else if (requested_right == right_direction)
        {
            right_reversal_pending = false;
        }
        else if ((requested_right == right_reversal_direction) &&
                 ((uint32_t)(
                      now_ms -
                      right_reversal_started_ms) >=
                  DRI0041_REVERSAL_BRAKE_MS))
        {
            right_reversal_pending = false;
            right_direction = requested_right;
        }
    }
    else
    {
        right_direction = requested_right;
    }

    return true;
}

bool motor_driver_stop(void)
{
    if (!initialized)
    {
        return false;
    }

    const Dri0041PortControl brake_control =
    {
        .left_duty = 0U,
        .left_in1 = false,
        .left_in2 = false,
        .right_duty = 0U,
        .right_in3 = false,
        .right_in4 = false
    };

    if (!motor_driver_dri0041_port_apply(&brake_control))
    {
        return false;
    }

    left_reversal_pending = false;
    right_reversal_pending = false;

    left_direction = DRI0041_DIRECTION_STOPPED;
    right_direction = DRI0041_DIRECTION_STOPPED;

    return true;
}