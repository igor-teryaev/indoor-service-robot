#ifndef UART_TX_PORT_H
#define UART_TX_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool uart_tx_port_write(
    const uint8_t *data,
    size_t size);

/*
 * Starts a best-effort non-blocking telemetry transmission.
 *
 * Returns false if the port is busy or transmission cannot start.
 * On success, the port must copy data into DMA-owned storage
 * before returning.
 */
bool uart_tx_port_try_write_telemetry(
    const uint8_t *data,
    size_t size);

#ifdef __cplusplus
}
#endif
#endif
