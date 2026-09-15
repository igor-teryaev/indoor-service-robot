#ifndef ROBOT_PROTOCOL_MOTION_ACK_PAYLOAD_H
#define ROBOT_PROTOCOL_MOTION_ACK_PAYLOAD_H

#include "motion_ack_status.h"

typedef struct
{
    MotionAckStatus status;
} MotionAckPayload;

#endif