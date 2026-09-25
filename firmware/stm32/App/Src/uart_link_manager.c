#include "uart_link_manager.h"

#include <stddef.h>

#include "link_sync_codec.h"
#include "motion_command_guard.h"
#include "protocol_ingress_router.h"
#include "protocol_message_type.h"
#include "uart_protocol_transmitter.h"
#include "heartbeat_codec.h"

typedef struct
{
    bool initialized;
    bool synchronized;
    uint32_t heartbeat_timeout_ms;
    uint32_t last_heartbeat_ms;
} UartLinkManagerState;

static UartLinkManagerState uart_link_manager_state;

static bool uart_link_manager_stop_and_clear(void)
{
    uart_link_manager_state.synchronized = false;
    return motion_command_guard_stop();
}

static bool uart_link_manager_send_heartbeat(
    uint16_t sequence,
    LinkState link_state,
    uint32_t now_ms)
{
    const HeartbeatPayload payload = {
        .link_state = link_state,
        .uptime_ms = now_ms
    };

    ProtocolFrame response = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = sequence,
        .payload_length = HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &payload,
        response.payload);

    return uart_protocol_transmitter_send(&response);
}

static UartLinkManagerResult uart_link_manager_handle_heartbeat(
    const ProtocolFrame *frame,
    uint32_t now_ms)
{
    if (protocol_ingress_route(frame) != PROTOCOL_INGRESS_ROUTE_HEARTBEAT)
    {
        if (!uart_link_manager_stop_and_clear())
        {
            return UART_LINK_MANAGER_RESULT_STOP_FAILED;
        }

        return UART_LINK_MANAGER_RESULT_INVALID_HEARTBEAT;
    }

    HeartbeatPayload payload = {0};
    heartbeat_decode(frame->payload, &payload);

    if (payload.link_state == LINK_STATE_UNSYNCHRONIZED)
    {
        if (!uart_link_manager_stop_and_clear())
        {
            return UART_LINK_MANAGER_RESULT_STOP_FAILED;
        }

        if (!uart_link_manager_send_heartbeat(
                frame->sequence,
                LINK_STATE_UNSYNCHRONIZED,
                now_ms))
        {
            return UART_LINK_MANAGER_RESULT_TRANSMIT_FAILED;
        }

        return UART_LINK_MANAGER_RESULT_PEER_UNSYNCHRONIZED;
    }

    if (payload.link_state != LINK_STATE_SYNCHRONIZED)
    {
        if (!uart_link_manager_stop_and_clear())
        {
            return UART_LINK_MANAGER_RESULT_STOP_FAILED;
        }

        return UART_LINK_MANAGER_RESULT_INVALID_HEARTBEAT;
    }

    if (!uart_link_manager_state.synchronized)
    {
        if (!uart_link_manager_send_heartbeat(
                frame->sequence,
                LINK_STATE_UNSYNCHRONIZED,
                now_ms))
        {
            return UART_LINK_MANAGER_RESULT_TRANSMIT_FAILED;
        }

        return UART_LINK_MANAGER_RESULT_NOT_SYNCHRONIZED;
    }

    if (!uart_link_manager_send_heartbeat(
            frame->sequence,
            LINK_STATE_SYNCHRONIZED,
            now_ms))
    {
        if (!uart_link_manager_stop_and_clear())
        {
            return UART_LINK_MANAGER_RESULT_STOP_FAILED;
        }

        return UART_LINK_MANAGER_RESULT_TRANSMIT_FAILED;
    }

    uart_link_manager_state.last_heartbeat_ms = now_ms;
    return UART_LINK_MANAGER_RESULT_HEARTBEAT_ACCEPTED;
}

static UartLinkManagerResult uart_link_manager_handle_link_sync(
    const ProtocolFrame *frame, uint32_t now_ms)
{
    if (frame->message_type != PROTOCOL_MESSAGE_TYPE_LINK_SYNC)
    {
        return UART_LINK_MANAGER_RESULT_NOT_LINK_MESSAGE;
    }

    if (!uart_link_manager_stop_and_clear())
    {
        return UART_LINK_MANAGER_RESULT_STOP_FAILED;
    }

    if (protocol_ingress_route(frame) != PROTOCOL_INGRESS_ROUTE_LINK_SYNC)
    {
        return UART_LINK_MANAGER_RESULT_INVALID_LINK_SYNC;
    }

    LinkSyncPayload payload = {0};
    link_sync_decode(frame->payload, &payload);

    ProtocolFrame response = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,
        .sequence = frame->sequence,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(&payload, response.payload);

    if (!uart_protocol_transmitter_send(&response))
    {
        return UART_LINK_MANAGER_RESULT_TRANSMIT_FAILED;
    }

    uart_link_manager_state.last_heartbeat_ms = now_ms;
    uart_link_manager_state.synchronized = true;
    return UART_LINK_MANAGER_RESULT_SYNCHRONIZED;
}

bool uart_link_manager_init(
    uint32_t heartbeat_timeout_ms)
{
    uart_link_manager_state =
        (UartLinkManagerState){0};

    if (heartbeat_timeout_ms == 0U)
    {
        return false;
    }

    uart_link_manager_state.heartbeat_timeout_ms =
        heartbeat_timeout_ms;
    uart_link_manager_state.initialized = true;

    return true;
}

bool uart_link_manager_is_synchronized(void)
{
    return uart_link_manager_state.synchronized;
}

UartLinkManagerResult uart_link_manager_handle(const ProtocolFrame *frame, uint32_t now_ms)
{
    if (frame == NULL)
    {
        return UART_LINK_MANAGER_RESULT_INVALID_ARGUMENT;
    }

    if (!uart_link_manager_state.initialized)
    {
        return UART_LINK_MANAGER_RESULT_NOT_INITIALIZED;
    }

    switch (frame->message_type)
    {
    case PROTOCOL_MESSAGE_TYPE_LINK_SYNC:
        return uart_link_manager_handle_link_sync(
            frame,
            now_ms);

    case PROTOCOL_MESSAGE_TYPE_HEARTBEAT:
        return uart_link_manager_handle_heartbeat(
            frame,
            now_ms);

    default:
        return UART_LINK_MANAGER_RESULT_NOT_LINK_MESSAGE;
    }
}

UartLinkManagerUpdate uart_link_manager_update(
    uint32_t now_ms)
{
    if (!uart_link_manager_state.initialized ||
        !uart_link_manager_state.synchronized)
    {
        return UART_LINK_MANAGER_UPDATE_NONE;
    }

    const uint32_t elapsed_ms =
        now_ms -
        uart_link_manager_state.last_heartbeat_ms;

    if (elapsed_ms <
        uart_link_manager_state.heartbeat_timeout_ms)
    {
        return UART_LINK_MANAGER_UPDATE_NONE;
    }

    if (!uart_link_manager_stop_and_clear())
    {
        return UART_LINK_MANAGER_UPDATE_STOP_FAILED;
    }

    return UART_LINK_MANAGER_UPDATE_LINK_LOST;
}