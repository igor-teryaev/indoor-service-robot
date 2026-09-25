#include "uart_rx_port.h"

#include <stddef.h>
#include <stdint.h>

#include "usart.h"

#define UART_RX_DMA_BUFFER_SIZE 128U

static UartRxQueue *rx_queue;
static uint8_t dma_buffer[UART_RX_DMA_BUFFER_SIZE];
static uint16_t previous_position;

static bool uart_rx_port_enqueue_range(
    uint16_t begin,
    uint16_t end)
{
    if ((rx_queue == NULL) ||
        (begin > end) ||
        (end > UART_RX_DMA_BUFFER_SIZE))
    {
        return false;
    }

    for (uint16_t i = begin; i < end; i++)
    {
        if (!uart_rx_queue_push(rx_queue, dma_buffer[i]))
        {
            return false;
        }

    }
    return true;
}

void HAL_UARTEx_RxEventCallback(
    UART_HandleTypeDef *huart,
    uint16_t size)
{
    if ((huart != &huart2) ||
        (rx_queue == NULL) ||
        (size > UART_RX_DMA_BUFFER_SIZE))
    {
        return;
    }

    if (size > previous_position)
    {
        (void)uart_rx_port_enqueue_range(previous_position, size);
    }
    else if (size < previous_position)
    {
        if (uart_rx_port_enqueue_range(previous_position, UART_RX_DMA_BUFFER_SIZE))
        {
            (void)uart_rx_port_enqueue_range(0U, size);
        }

    }

    previous_position = size;
}

bool uart_rx_port_init(UartRxQueue *queue)
{
    if ((queue == NULL) ||
        (huart2.Instance != USART2))
    {
        return false;
    }

    uart_rx_queue_init(queue);

    previous_position = 0U;

    rx_queue = queue;

    if (HAL_UARTEx_ReceiveToIdle_DMA(
        &huart2,
        dma_buffer,
        UART_RX_DMA_BUFFER_SIZE) != HAL_OK)
    {
        rx_queue = NULL;
        return false;
    }

    return true;
}