#ifndef WHEEL_ENCODER_PORT_H
#define WHEEL_ENCODER_PORT_H

#include <stdbool.h>

#include "wheel_encoder.h"

bool wheel_encoder_port_init(void);
WheelEncoderCounts wheel_encoder_port_read(void);

#endif