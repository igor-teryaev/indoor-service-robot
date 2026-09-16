#ifndef ROBOT_PROTOCOL_MOTION_SEQUENCE_H
#define ROBOT_PROTOCOL_MOTION_SEQUENCE_H

#include <stdint.h>

typedef enum
{
    MOTION_SEQUENCE_SAME,
    MOTION_SEQUENCE_NEWER,
    MOTION_SEQUENCE_STALE
} MotionSequenceRelation;

#ifdef __cplusplus
extern "C" {
#endif

MotionSequenceRelation motion_sequence_classify(
    uint16_t reference_sequence,
    uint16_t received_sequence);

#ifdef __cplusplus
}
#endif

#endif