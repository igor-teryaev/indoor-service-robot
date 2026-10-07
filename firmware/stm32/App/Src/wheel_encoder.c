#include "wheel_encoder.h"

#include "wheel_encoder_port.h"

bool wheel_encoder_init(void)
{
    return wheel_encoder_port_init();
}

WheelEncoderCounts wheel_encoder_read(void)
{
    return wheel_encoder_port_read();
}