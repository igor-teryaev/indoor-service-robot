#ifndef UART_PROTOCOL_RECEIVER_H
#define UART_PROTOCOL_RECEIVER_H

#include <stdbool.h>

#include "protocol_frame_decoder.h"
#include "uart_rx_queue.h"

typedef enum
{
    UART_PROTOCOL_RECEIVER_RESULT_NONE = 0,
    UART_PROTOCOL_RECEIVER_RESULT_FRAME,
    UART_PROTOCOL_RECEIVER_RESULT_OVERFLOW,
    UART_PROTOCOL_RECEIVER_RESULT_INVALID_ARGUMENT
} UartProtocolReceiverResult;

typedef struct
{
    UartRxQueue *queue;
    ProtocolFrameDecoder decoder;
} UartProtocolReceiver;

#ifdef __cplusplus
extern "C" {
#endif

bool uart_protocol_receiver_init(
    UartProtocolReceiver *receiver,
    UartRxQueue *queue);

UartProtocolReceiverResult uart_protocol_receiver_poll(
    UartProtocolReceiver *receiver,
    const ProtocolFrame **frame);

#ifdef __cplusplus
}
#endif
#endif
