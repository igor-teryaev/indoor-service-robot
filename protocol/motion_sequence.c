#include "motion_sequence.h"


MotionSequenceRelation motion_sequence_classify(
    uint16_t reference_sequence,
    uint16_t received_sequence)
{
    const uint16_t delta = (uint16_t)(received_sequence - reference_sequence);

    if (delta == 0U)
    {
        return MOTION_SEQUENCE_SAME;
    }

    if (delta < 0x8000U)
    {
        return MOTION_SEQUENCE_NEWER;
    }

    return MOTION_SEQUENCE_STALE;
}