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

#ifdef __cplusplus
}
#endif
#endif
