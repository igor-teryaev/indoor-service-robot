#ifndef UART_PROTOCOL_TRANSMITTER_H
#define UART_PROTOCOL_TRANSMITTER_H

#include <stdbool.h>

#include "protocol_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

bool uart_protocol_transmitter_send(
    const ProtocolFrame *frame);

#ifdef __cplusplus
}
#endif

#endif