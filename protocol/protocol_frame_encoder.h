#ifndef ROBOT_PROTOCOL_PROTOCOL_FRAME_ENCODER_H
#define ROBOT_PROTOCOL_PROTOCOL_FRAME_ENCODER_H

#include <stddef.h>
#include <stdint.h>

#include "protocol_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t protocol_frame_encode(
    const ProtocolFrame* frame,
    uint8_t* output);

#ifdef __cplusplus
}
#endif

#endif