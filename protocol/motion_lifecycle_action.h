#ifndef ROBOT_PROTOCOL_MOTION_LIFECYCLE_ACTION_H
#define ROBOT_PROTOCOL_MOTION_LIFECYCLE_ACTION_H

#include <stdbool.h>
#include <stdint.h>

#include "motion_lifecycle_operation.h"
#include "motion_response_result.h"
#include "motion_transaction.h"

typedef struct
{
    bool send_ack;
    MotionTransaction ack_transaction;

    bool send_response;
    MotionTransaction response_transaction;
    MotionResponseResult response_result;

    MotionLifecycleOperation operation;

    uint32_t operation_id;
} MotionLifecycleAction;

#ifdef __cplusplus
extern "C" {
#endif

MotionLifecycleAction motion_lifecycle_action_none(void);

#ifdef __cplusplus
}
#endif

#endif