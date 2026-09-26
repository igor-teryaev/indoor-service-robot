#include "wheel_velocity_feedforward.h"

#include <stddef.h>

static bool wheel_velocity_feedforward_convert_one(
    const WheelVelocityFeedforwardConfig *config,
    int16_t velocity_mm_s,
    int16_t *motor_command)
{
    if ((config == NULL) ||
        (motor_command == NULL) ||
        (config->max_velocity_mm_s == 0U) ||
        (config->minimum_start_command >
         MOTOR_DRIVER_COMMAND_MAX))
    {
        return false;
    }

    if (velocity_mm_s == 0)
    {
        *motor_command = 0;
        return true;
    }

    /*
     * Convert through int32_t first.
     *
     * This is important for INT16_MIN:
     *
     *     int16_t = -32768
     *
     * Its positive magnitude, 32768, cannot be represented
     * by int16_t.
     */
    const int32_t signed_velocity =
        (int32_t)velocity_mm_s;

    uint32_t magnitude =
        (signed_velocity < 0)
            ? (uint32_t)(-signed_velocity)
            : (uint32_t)signed_velocity;

    /*
     * Requests above the calibrated open-loop range
     * saturate at maximum motor command.
     */
    if (magnitude > config->max_velocity_mm_s)
    {
        magnitude = config->max_velocity_mm_s;
    }

    const uint32_t command_range =
        (uint32_t)MOTOR_DRIVER_COMMAND_MAX -
        config->minimum_start_command;

    const uint32_t command_magnitude =
        config->minimum_start_command +
        ((magnitude * command_range) /
         config->max_velocity_mm_s);

    *motor_command =
        (signed_velocity < 0)
            ? -(int16_t)command_magnitude
            : (int16_t)command_magnitude;

    return true;
}

bool wheel_velocity_feedforward_convert(
    const WheelVelocityFeedforwardConfig *config,
    const WheelVelocityCommand *velocity,
    MotorDriverCommand *motor_command)
{
    if ((config == NULL) ||
        (velocity == NULL) ||
        (motor_command == NULL))
    {
        return false;
    }

    MotorDriverCommand result = {0};

    if (!wheel_velocity_feedforward_convert_one(
            config,
            velocity->left_velocity_mm_s,
            &result.left))
    {
        return false;
    }

    if (!wheel_velocity_feedforward_convert_one(
            config,
            velocity->right_velocity_mm_s,
            &result.right))
    {
        return false;
    }

    *motor_command = result;

    return true;
}