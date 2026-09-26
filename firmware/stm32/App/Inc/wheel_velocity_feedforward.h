#ifndef WHEEL_VELOCITY_FEEDFORWARD_H
#define WHEEL_VELOCITY_FEEDFORWARD_H

#include <stdbool.h>
#include <stdint.h>

#include "motor_driver.h"
#include "wheel_velocity_command.h"

typedef struct
{
    /*
     * Requested velocity magnitude that maps to
     * MOTOR_DRIVER_COMMAND_MAX.
     *
     * This is an open-loop calibration value,
     * not a guaranteed physical velocity.
     */
    uint16_t max_velocity_mm_s;

    /*
     * Minimum non-zero motor command used to overcome
     * the ARC101 motor startup dead zone.
     */
    uint16_t minimum_start_command;

} WheelVelocityFeedforwardConfig;

#ifdef __cplusplus
extern "C" {
#endif

bool wheel_velocity_feedforward_convert(
    const WheelVelocityFeedforwardConfig *config,
    const WheelVelocityCommand *velocity,
    MotorDriverCommand *motor_command);

#ifdef __cplusplus
}
#endif

#endif