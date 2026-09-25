#ifndef UART_RX_QUEUE_H
#define UART_RX_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#define UART_RX_QUEUE_CAPACITY 256U


typedef struct
{
    volatile uint8_t data[UART_RX_QUEUE_CAPACITY];
    volatile uint32_t read_count;
    volatile uint32_t write_count;
    volatile bool overflowed;
} UartRxQueue;

#ifdef __cplusplus
extern "C" {
#endif

void uart_rx_queue_init(UartRxQueue *queue);
bool uart_rx_queue_push(UartRxQueue *queue, uint8_t byte);
bool uart_rx_queue_pop(UartRxQueue *queue, uint8_t *byte);
bool uart_rx_queue_has_overflowed(const UartRxQueue *queue);
void uart_rx_queue_reset(UartRxQueue *queue);

#ifdef __cplusplus
}
#endif
#endif
