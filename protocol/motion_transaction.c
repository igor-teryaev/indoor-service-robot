#include "motion_transaction.h"

#include <stddef.h>

bool motion_transaction_equal(
    const MotionTransaction* first,
    const MotionTransaction* second)
{
    if ((first == NULL) || (second == NULL))
    {
        return false;
    }

    return
        (first->sequence == second->sequence) &&
        (first->command == second->command) &&
        (first->motion_session_id == second->motion_session_id);
}