#pragma once

#include <cstdint>
#include <optional>

#include "motion_response_result.h"
#include "protocol_frame.h"
#include "motion_lifecycle_command_type.h"

class Stm32MotionSession
{
public:
    enum class State
    {
        Inactive,
        StartPending,
        Active,
        EndPending
    };
    [[nodiscard]] State state() const;
    [[nodiscard]] std::optional<ProtocolFrame> begin_start_session(std::uint32_t motion_session_id);
    [[nodiscard]] std::optional<ProtocolFrame> begin_end_session(std::uint32_t motion_session_id);
    [[nodiscard]] bool handle_ack(const ProtocolFrame& frame, std::uint32_t now_ms);
    [[nodiscard]] std::optional<MotionResponseResult> handle_response(const ProtocolFrame& frame);
    [[nodiscard]] std::optional<ProtocolFrame> retry_pending_transaction() const;

    void mark_pending_transmitted(std::uint32_t now_ms);
    void reset();

    [[nodiscard]] std::optional<ProtocolFrame> retry_if_due(std::uint32_t now_ms);

    [[nodiscard]] bool retry_exhausted(std::uint32_t now_ms) const;

    [[nodiscard]] std::optional<ProtocolFrame> build_wheel_velocity(
            std::int16_t left_velocity_mm_s,
            std::int16_t right_velocity_mm_s);
private:
    std::uint16_t next_sequence_ = 1U;
    std::uint16_t pending_sequence_ = 0U;
    std::uint32_t pending_session_id_ = 0U;
    bool transaction_pending_ = false;
    MotionLifecycleCommandType pending_command_ = 0U;
    ProtocolFrame pending_frame_ = {};

    bool ack_received_ = false;
    bool retry_timer_running_ = false;

    std::uint8_t retry_count_ = 0U;
    std::uint32_t last_transmit_ms_ = 0U;

    static constexpr std::uint32_t ack_timeout_ms_ = 100U;
    static constexpr std::uint32_t terminal_timeout_ms_ = 750U;
    static constexpr std::uint8_t max_retries_ = 3U;

    std::uint16_t next_wheel_sequence_ = 1U;

    State state_ = State::Inactive;
    std::uint32_t active_session_id_ = 0U;
};