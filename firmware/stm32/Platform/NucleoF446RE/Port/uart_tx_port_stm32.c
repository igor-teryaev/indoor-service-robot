#include "usart.h"
#include "uart_tx_port.h"

#include <string.h>


#define UART_TX_TIMEOUT_MS              20U
#define UART_TELEMETRY_DRAIN_TIMEOUT_MS 5U
#define UART_TELEMETRY_WIRE_SIZE        15U

bool uart_tx_port_write(const uint8_t *data, size_t size)
{
    if ((data == NULL) ||
        (huart2.Instance != USART2) ||
        (size == 0U) ||
        (size > UINT16_MAX))
    {
        return false;
    }

    const uint32_t start_ms = HAL_GetTick();

    while (huart2.gState != HAL_UART_STATE_READY)
    {
        if ((uint32_t)(HAL_GetTick() - start_ms) >=
            UART_TELEMETRY_DRAIN_TIMEOUT_MS)
        {
            return false;
        }
    }

    return HAL_UART_Transmit(
               &huart2,
               (uint8_t *)data,
               (uint16_t)size,
               UART_TX_TIMEOUT_MS) == HAL_OK;
}

static uint8_t telemetry_dma_buffer[UART_TELEMETRY_WIRE_SIZE];

bool uart_tx_port_try_write_telemetry(
    const uint8_t *data,
    size_t size)
{
    if ((data == NULL) ||
        (huart2.Instance != USART2) ||
        (size == 0U) ||
        (size != sizeof(telemetry_dma_buffer)) ||
        (huart2.gState != HAL_UART_STATE_READY))
    {
        return false;
    }

    memcpy(telemetry_dma_buffer, data, size);

    return HAL_UART_Transmit_DMA(
               &huart2,
               telemetry_dma_buffer,
               (uint16_t)size) == HAL_OK;
}