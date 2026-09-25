#ifndef WHEEL_ENCODER_H
#define WHEEL_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t left;
    uint32_t right;
} WheelEncoderCounts;

bool wheel_encoder_init(void);
WheelEncoderCounts wheel_encoder_read(void);

#endif