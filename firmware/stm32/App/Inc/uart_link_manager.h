#ifndef UART_LINK_MANAGER_H
#define UART_LINK_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol_frame.h"

typedef enum
{
    UART_LINK_MANAGER_RESULT_INVALID_ARGUMENT = 0,
    UART_LINK_MANAGER_RESULT_NOT_INITIALIZED,
    UART_LINK_MANAGER_RESULT_NOT_LINK_MESSAGE,
    UART_LINK_MANAGER_RESULT_INVALID_LINK_SYNC,
    UART_LINK_MANAGER_RESULT_INVALID_HEARTBEAT,
    UART_LINK_MANAGER_RESULT_NOT_SYNCHRONIZED,
    UART_LINK_MANAGER_RESULT_STOP_FAILED,
    UART_LINK_MANAGER_RESULT_TRANSMIT_FAILED,
    UART_LINK_MANAGER_RESULT_SYNCHRONIZED,
    UART_LINK_MANAGER_RESULT_HEARTBEAT_ACCEPTED,
    UART_LINK_MANAGER_RESULT_PEER_UNSYNCHRONIZED
} UartLinkManagerResult;

typedef enum
{
    UART_LINK_MANAGER_UPDATE_NONE = 0,
    UART_LINK_MANAGER_UPDATE_LINK_LOST,
    UART_LINK_MANAGER_UPDATE_STOP_FAILED
} UartLinkManagerUpdate;

#ifdef __cplusplus
extern "C" {
#endif

bool uart_link_manager_init(
    uint32_t heartbeat_timeout_ms);

UartLinkManagerResult uart_link_manager_handle(
    const ProtocolFrame *frame,
    uint32_t now_ms);

UartLinkManagerUpdate uart_link_manager_update(uint32_t now_ms);

bool uart_link_manager_is_synchronized(void);

#ifdef __cplusplus
}
#endif

#endif