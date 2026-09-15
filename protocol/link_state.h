#ifndef ROBOT_PROTOCOL_LINK_STATE_H
#define ROBOT_PROTOCOL_LINK_STATE_H

#include <stdint.h>

typedef uint8_t LinkState;

#define LINK_STATE_UNSYNCHRONIZED 0x00U
#define LINK_STATE_SYNCHRONIZED   0x01U

#endif