#include "stm32_motion_session.h"

extern "C"
{
#include "motion_lifecycle_command_codec.h"
#include "motion_lifecycle_command_type.h"
#include "protocol_message_type.h"
#include "motion_ack_codec.h"
#include "motion_response_codec.h"
}

std::optional<ProtocolFrame>Stm32MotionSession::begin_start_session(const std::uint32_t motion_session_id)
{
    if (transaction_pending_)
    {
        return std::nullopt;
    }

    const MotionLifecycleCommandPayload payload =
    {
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = motion_session_id
    };

    ProtocolFrame frame =
    {
        .message_type = PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND,
        .sequence = next_sequence_++,
        .payload_length = MOTION_LIFECYCLE_COMMAND_WIRE_SIZE
    };

    motion_lifecycle_command_encode(
        &payload,
        frame.payload);

    pending_sequence_ = frame.sequence;
    pending_session_id_ = motion_session_id;
    pending_command_ = MOTION_LIFECYCLE_COMMAND_START_SESSION;
    pending_frame_ = frame;

    ack_received_ = false;
    retry_timer_running_ = false;
    retry_count_ = 0U;
    transaction_pending_ = true;

    return frame;
}

bool Stm32MotionSession::handle_ack(const ProtocolFrame& frame, std::uint32_t now_ms)
{
    if (!transaction_pending_)
    {
        return false;
    }

    if (frame.message_type !=
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK)
    {
        return false;
    }

    if (frame.sequence != pending_sequence_)
    {
        return false;
    }

    if (frame.payload_length !=
        MOTION_ACK_WIRE_SIZE)
    {
        return false;
    }

    MotionAckPayload payload = {};

    motion_ack_decode(
        frame.payload,
        &payload);

    if (payload.status != MOTION_ACK_ACCEPTED)
    {
        return false;
    }

    if (!ack_received_)
    {
        ack_received_ = true;
        last_transmit_ms_ = now_ms;
    }

    return true;
}

std::optional<MotionResponseResult>Stm32MotionSession::handle_response(const ProtocolFrame& frame)
{
    if (!transaction_pending_)
    {
        return std::nullopt;
    }

    if (frame.message_type != PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE)
    {
        return std::nullopt;
    }

    if (frame.sequence != pending_sequence_)
    {
        return std::nullopt;
    }

    if (frame.payload_length != MOTION_RESPONSE_WIRE_SIZE)
    {
        return std::nullopt;
    }

    MotionResponsePayload payload = {};

    motion_response_decode(
        frame.payload,
        &payload);

    if (payload.command != pending_command_)
    {
        return std::nullopt;
    }

    if (payload.motion_session_id != pending_session_id_)
    {
        return std::nullopt;
    }

    transaction_pending_ = false;
    retry_timer_running_ = false;

    return payload.result;
}

std::optional<ProtocolFrame>Stm32MotionSession::begin_end_session(const std::uint32_t motion_session_id)
{
    if (transaction_pending_)
    {
        return std::nullopt;
    }

    const MotionLifecycleCommandPayload payload =
    {
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = motion_session_id
    };

    ProtocolFrame frame =
    {
        .message_type = PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND,
        .sequence = next_sequence_++,
        .payload_length = MOTION_LIFECYCLE_COMMAND_WIRE_SIZE
    };

    motion_lifecycle_command_encode(
        &payload,
        frame.payload);

    pending_sequence_ = frame.sequence;
    pending_session_id_ = motion_session_id;
    pending_command_ = MOTION_LIFECYCLE_COMMAND_END_SESSION;
    pending_frame_ = frame;

    ack_received_ = false;
    retry_timer_running_ = false;
    retry_count_ = 0U;
    transaction_pending_ = true;

    return frame;
}

std::optional<ProtocolFrame>Stm32MotionSession::retry_pending_transaction() const
{
    if (!transaction_pending_)
    {
        return std::nullopt;
    }

    return pending_frame_;
}

void Stm32MotionSession::mark_pending_transmitted(const std::uint32_t now_ms)
{
    if (!transaction_pending_)
    {
        return;
    }

    last_transmit_ms_ = now_ms;
    retry_timer_running_ = true;
}

std::optional<ProtocolFrame>Stm32MotionSession::retry_if_due(const std::uint32_t now_ms)
{
    if (!transaction_pending_ ||
        !retry_timer_running_)
    {
        return std::nullopt;
    }

    const std::uint32_t timeout_ms =
        ack_received_
            ? terminal_timeout_ms_
            : ack_timeout_ms_;

    if (now_ms - last_transmit_ms_ <
        timeout_ms)
    {
        return std::nullopt;
    }

    if (retry_count_ >= max_retries_)
    {
        return std::nullopt;
    }

    ++retry_count_;

    /*
     * Do not start another timeout until the caller
     * confirms that this retry was actually transmitted.
     */
    retry_timer_running_ = false;

    return pending_frame_;
}

bool Stm32MotionSession::retry_exhausted(const std::uint32_t now_ms) const
{
    if (!transaction_pending_ ||
        !retry_timer_running_)
    {
        return false;
    }

    if (retry_count_ < max_retries_)
    {
        return false;
    }

    const std::uint32_t timeout_ms =
        ack_received_
            ? terminal_timeout_ms_
            : ack_timeout_ms_;

    return now_ms - last_transmit_ms_ >=
        timeout_ms;
}

void Stm32MotionSession::reset()
{
    pending_sequence_ = 0U;
    pending_session_id_ = 0U;
    pending_command_ = 0U;
    pending_frame_ = {};

    transaction_pending_ = false;
    ack_received_ = false;
    retry_timer_running_ = false;

    retry_count_ = 0U;
    last_transmit_ms_ = 0U;
}