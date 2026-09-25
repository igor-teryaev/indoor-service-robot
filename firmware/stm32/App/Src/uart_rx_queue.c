#include "uart_rx_queue.h"
#include <stddef.h>

#if (UART_RX_QUEUE_CAPACITY == 0U) || \
    ((UART_RX_QUEUE_CAPACITY & (UART_RX_QUEUE_CAPACITY - 1U)) != 0U)
#error "UART_RX_QUEUE_CAPACITY must be a power of two"
#endif

void uart_rx_queue_init(UartRxQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    *queue = (UartRxQueue){0};
}

void uart_rx_queue_reset(UartRxQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    queue->overflowed = true;
    queue->read_count = queue->write_count;
    queue->overflowed = false;
}

bool uart_rx_queue_push(UartRxQueue *queue, uint8_t byte)
{
    if ((queue == NULL) ||
        (queue->overflowed))
    {
        return false;
    }

    const uint32_t used = queue->write_count - queue->read_count;
    if (used >= UART_RX_QUEUE_CAPACITY)
    {
        queue->overflowed = true;
        return false;
    }

    const uint32_t index =
        queue->write_count &
            (UART_RX_QUEUE_CAPACITY - 1U);

    queue->data[index] = byte;
    queue->write_count++;
    return true;
}

bool uart_rx_queue_pop(UartRxQueue *queue, uint8_t *byte)
{
    if ((queue == NULL) ||
        (byte == NULL) ||
        (queue->overflowed))
    {
        return false;
    }

    if (queue->read_count == queue->write_count)
    {
        return false;
    }

    const uint32_t index =
        queue->read_count &
            (UART_RX_QUEUE_CAPACITY - 1U);
    *byte = queue->data[index];
    queue->read_count++;
    return true;
}

bool uart_rx_queue_has_overflowed(const UartRxQueue *queue)
{
    if (queue == NULL)
    {
        return false;
    }

    return queue->overflowed;
}