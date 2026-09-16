#ifndef ROBOT_PROTOCOL_PROTOCOL_FRAME_H
#define ROBOT_PROTOCOL_PROTOCOL_FRAME_H

#include <stdint.h>

#include "protocol_message_type.h"
#include "protocol_frame_format.h"

typedef struct
{
    ProtocolMessageType message_type;
    uint16_t sequence;
    uint16_t payload_length;
    uint8_t payload[PROTOCOL_FRAME_MAX_PAYLOAD_SIZE];
} ProtocolFrame;

#endif