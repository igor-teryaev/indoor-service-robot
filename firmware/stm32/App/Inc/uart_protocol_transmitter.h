#ifndef UART_PROTOCOL_TRANSMITTER_H
#define UART_PROTOCOL_TRANSMITTER_H

#include <stdbool.h>

#include "protocol_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

bool uart_protocol_transmitter_send(
    const ProtocolFrame *frame);

/*
 * Attempts to transmit a telemetry frame without blocking.
 *
 * Returns true if transmission was started.
 * Returns false if the port is busy or transmission cannot start.
 *
 * The underlying port must copy the encoded frame into
 * DMA-owned storage before returning.
 */
bool uart_protocol_transmitter_try_send_telemetry(
    const ProtocolFrame *frame);

#ifdef __cplusplus
}
#endif

#endif