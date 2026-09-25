#include "usart.h"
#include "uart_tx_port.h"

#define UART_TX_TIMEOUT_MS 20U

bool uart_tx_port_write(const uint8_t *data, size_t size)
{
    if ((data == NULL) ||
        (huart2.Instance != USART2) ||
        (size == 0U) ||
        (size > UINT16_MAX))
    {
        return false;
    }

    return HAL_UART_Transmit(
               &huart2,
               (uint8_t *)data,
               (uint16_t)size,
               UART_TX_TIMEOUT_MS) == HAL_OK;
}