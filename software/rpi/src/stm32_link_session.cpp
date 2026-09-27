#include "stm32_link_session.h"

extern "C"
{
#include "link_sync_codec.h"
#include "protocol_message_type.h"
#include "heartbeat_codec.h"
}

Stm32LinkState Stm32LinkSession::state() const
{
    return state_;
}

std::optional<ProtocolFrame> Stm32LinkSession::heartbeat_if_due(std::uint32_t now_ms)
{
    if (state_ != Stm32LinkState::Synchronized)
    {
        return std::nullopt;
    }

    const std::uint32_t elapsed_ms =
        now_ms - last_heartbeat_tx_ms_;

    if (elapsed_ms < heartbeat_period_ms_)
    {
        return std::nullopt;
    }

    const HeartbeatPayload payload =
    {
        .link_state = LINK_STATE_SYNCHRONIZED,
        .uptime_ms = now_ms
    };

    const std::uint16_t sequence = next_sequence_++;

    pending_heartbeat_sequence_ = sequence;

    heartbeat_response_pending_ = true;

    ProtocolFrame frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence = sequence,

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &payload,
        frame.payload);

    last_heartbeat_tx_ms_ = now_ms;

    return frame;
}

ProtocolFrame Stm32LinkSession::begin_synchronization(std::uint64_t sync_token)
{
    heartbeat_response_pending_ = false;

    const std::uint16_t sequence = next_sequence_++;

    pending_sync_sequence_ = sequence;
    pending_sync_token_ = sync_token;

    state_ = Stm32LinkState::SyncPending;

    const LinkSyncPayload payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC,

        .sequence = sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &payload,
        frame.payload);

    return frame;
}

bool Stm32LinkSession::handle_link_sync_ok(
    const ProtocolFrame& frame,
    std::uint32_t now_ms)
{
    if (state_ != Stm32LinkState::SyncPending)
    {
        return false;
    }

    if (frame.message_type !=
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK)
    {
        return false;
    }

    if (frame.payload_length !=
        LINK_SYNC_WIRE_SIZE)
    {
        return false;
    }

    if (frame.sequence !=
        pending_sync_sequence_)
    {
        return false;
    }

    LinkSyncPayload payload = {};

    link_sync_decode(
        frame.payload,
        &payload);

    if (payload.sync_token !=
        pending_sync_token_)
    {
        return false;
    }

    state_ = Stm32LinkState::Synchronized;

    last_heartbeat_tx_ms_ = now_ms;
    last_valid_response_ms_ = now_ms;

    return true;
}

bool Stm32LinkSession::handle_heartbeat_response(
    const ProtocolFrame& frame,
    std::uint32_t now_ms)
{
    if (!heartbeat_response_pending_)
    {
        return false;
    }

    if (state_ != Stm32LinkState::Synchronized)
    {
        return false;
    }

    if (frame.message_type !=
        PROTOCOL_MESSAGE_TYPE_HEARTBEAT)
    {
        return false;
    }

    if (frame.payload_length !=
        HEARTBEAT_WIRE_SIZE)
    {
        return false;
    }

    if (frame.sequence !=
        pending_heartbeat_sequence_)
    {
        return false;
    }

    HeartbeatPayload payload = {};

    heartbeat_decode(
        frame.payload,
        &payload);

    if (payload.link_state == LINK_STATE_UNSYNCHRONIZED)
    {
        heartbeat_response_pending_ = false;
        state_ = Stm32LinkState::Disconnected;
        return false;
    }

    if (payload.link_state != LINK_STATE_SYNCHRONIZED)
    {
        return false;
    }

    heartbeat_response_pending_ = false;

    last_valid_response_ms_ =  now_ms;

    return true;
}

void Stm32LinkSession::check_link_timeout(std::uint32_t now_ms)
{
    if (state_ != Stm32LinkState::Synchronized)
    {
        return;
    }

    const std::uint32_t elapsed_ms =
        now_ms - last_valid_response_ms_;

    if (elapsed_ms >= link_timeout_ms_)
    {
        heartbeat_response_pending_ = false;
        state_ = Stm32LinkState::Disconnected;
    }
}

void Stm32LinkSession::disconnect()
{
    state_ = Stm32LinkState::Disconnected;

    heartbeat_response_pending_ = false;

    pending_sync_sequence_ = 0U;
    pending_sync_token_ = 0U;

    last_heartbeat_tx_ms_ = 0U;
    last_valid_response_ms_ = 0U;
}