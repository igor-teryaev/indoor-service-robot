#include <stdbool.h>
#include <stdint.h>

#include "motion_lifecycle_command_type.h"

typedef struct
{
    uint16_t sequence;
    MotionLifecycleCommandType command;
    uint32_t motion_session_id;
} MotionTransaction;

#ifdef __cplusplus
extern "C" {
#endif

bool motion_transaction_equal(
    const MotionTransaction* first,
    const MotionTransaction* second);

#ifdef __cplusplus
}
#endif