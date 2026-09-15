#include "motion_ack_codec.h"

void motion_ack_encode(
    const MotionAckPayload* payload,
    uint8_t output[MOTION_ACK_WIRE_SIZE])
{
    output[0] = payload->status;
}

void motion_ack_decode(
    const uint8_t input[MOTION_ACK_WIRE_SIZE],
    MotionAckPayload* payload)
{
    payload->status = input[0];
}
