#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_DRIVER_COMMAND_MIN (-1000)
#define MOTOR_DRIVER_COMMAND_MAX 1000

typedef struct
{
    int16_t left;
    int16_t right;
} MotorDriverCommand;

bool motor_driver_init(void);
bool motor_driver_apply(const MotorDriverCommand *command);
bool motor_driver_stop(void);

#endif