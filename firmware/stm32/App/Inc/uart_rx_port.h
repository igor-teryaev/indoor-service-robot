#ifndef UART_RX_PORT_H
#define UART_RX_PORT_H

#include <stdbool.h>

#include "uart_rx_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

bool uart_rx_port_init(UartRxQueue *queue);

#ifdef __cplusplus
}
#endif
#endif