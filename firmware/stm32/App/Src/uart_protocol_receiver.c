#include "uart_protocol_receiver.h"

#include <stddef.h>

static bool uart_protocol_receiver_handle_overflow(
    UartProtocolReceiver *receiver)
{
    if (!uart_rx_queue_has_overflowed(receiver->queue))
    {
        return false;
    }

    uart_rx_queue_reset(receiver->queue);
    protocol_frame_decoder_reset(&receiver->decoder);
    return true;
}

bool uart_protocol_receiver_init(
    UartProtocolReceiver *receiver,
    UartRxQueue *queue)
{
    if (receiver == NULL)
    {
        return false;
    }
    *receiver = (UartProtocolReceiver){0};

    if (queue == NULL)
    {
        return false;
    }

    protocol_frame_decoder_init(&receiver->decoder);
    receiver->queue = queue;

    return true;
}

UartProtocolReceiverResult uart_protocol_receiver_poll(
    UartProtocolReceiver *receiver,
    const ProtocolFrame **frame)
{
    if (frame == NULL)
    {
        return UART_PROTOCOL_RECEIVER_RESULT_INVALID_ARGUMENT;
    }

    *frame = NULL;

    if ((receiver == NULL) ||
        (receiver->queue == NULL))
    {
        return UART_PROTOCOL_RECEIVER_RESULT_INVALID_ARGUMENT;
    }

    if (uart_protocol_receiver_handle_overflow(receiver))
    {
        return UART_PROTOCOL_RECEIVER_RESULT_OVERFLOW;
    }

    uint8_t byte;
    const ProtocolFrame *decoded_frame = NULL;

    while (uart_rx_queue_pop(receiver->queue, &byte))
    {
        if (protocol_frame_decoder_feed_byte(&receiver->decoder, byte, &decoded_frame))
        {
            if (uart_protocol_receiver_handle_overflow(receiver))
            {
                return UART_PROTOCOL_RECEIVER_RESULT_OVERFLOW;
            }

            *frame = decoded_frame;
            return UART_PROTOCOL_RECEIVER_RESULT_FRAME;
        }
    }

    if (uart_protocol_receiver_handle_overflow(receiver))
    {
        return UART_PROTOCOL_RECEIVER_RESULT_OVERFLOW;
    }

    return UART_PROTOCOL_RECEIVER_RESULT_NONE;
}