#ifndef ROBOT_PROTOCOL_CRC16_CCITT_FALSE_H
#define ROBOT_PROTOCOL_CRC16_CCITT_FALSE_H

#include <stddef.h>
#include <stdint.h>

#define CRC16_CCITT_FALSE_INITIAL 0xFFFFU

#ifdef __cplusplus
extern "C" {
#endif

uint16_t crc16_ccitt_false_update(
    uint16_t crc,
    const uint8_t* data,
    size_t length);

uint16_t crc16_ccitt_false(
    const uint8_t* data,
    size_t length);

#ifdef __cplusplus
}
#endif

#endif