#include "uart_protocol_transmitter.h"

#include <stddef.h>
#include <stdint.h>

#include "protocol_frame_encoder.h"
#include "uart_tx_port.h"

bool uart_protocol_transmitter_send(
    const ProtocolFrame *frame)
{
    if (frame == NULL)
    {
        return false;
    }

    uint8_t wire[PROTOCOL_FRAME_MAX_WIRE_SIZE] = {0};
    const size_t wire_size = protocol_frame_encode(frame, wire);
    if (wire_size == 0U)
    {
        return false;
    }

    return uart_tx_port_write(wire, wire_size);
}