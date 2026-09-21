#ifndef MOTOR_DRIVER_PORT_H
#define MOTOR_DRIVER_PORT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint16_t ain1_duty;
    uint16_t ain2_duty;
    uint16_t bin1_duty;
    uint16_t bin2_duty;
} MotorDriverPortControl;

bool motor_driver_port_init(void);
bool motor_driver_port_apply(const MotorDriverPortControl *control);

#endif