#ifndef ROBOT_PROTOCOL_PROTOCOL_FRAME_ENCODER_H
#define ROBOT_PROTOCOL_PROTOCOL_FRAME_ENCODER_H

#include <stddef.h>
#include <stdint.h>

#include "protocol_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Encodes a ProtocolFrame into output.
 *
 * Preconditions:
 * - frame must not be NULL;
 * - output must point to writable memory large enough for the
 *   encoded frame (PROTOCOL_FRAME_MAX_WIRE_SIZE is always sufficient);
 * - frame and output memory must not overlap.
 *
 * Returns the encoded frame size, or 0 if payload_length exceeds
 * PROTOCOL_FRAME_MAX_PAYLOAD_SIZE.
 */
size_t protocol_frame_encode(
    const ProtocolFrame* frame,
    uint8_t* output);

#ifdef __cplusplus
}
#endif

#endif