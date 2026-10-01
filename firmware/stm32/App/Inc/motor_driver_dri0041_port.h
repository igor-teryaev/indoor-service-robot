#ifndef MOTOR_DRIVER_DRI0041_PORT_H
#define MOTOR_DRIVER_DRI0041_PORT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint16_t left_duty;
    bool left_in1;
    bool left_in2;

    uint16_t right_duty;
    bool right_in3;
    bool right_in4;
} Dri0041PortControl;

bool motor_driver_dri0041_port_init(void);

bool motor_driver_dri0041_port_apply(const Dri0041PortControl *control);

uint32_t motor_driver_dri0041_port_now_ms(void);

#endif