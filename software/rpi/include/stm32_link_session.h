#pragma once

#include <cstdint>
#include <optional>

#include "protocol_frame.h"

enum class Stm32LinkState
{
    Disconnected,
    SyncPending,
    Synchronized
};

class Stm32LinkSession
{
public:
    [[nodiscard]] Stm32LinkState state() const;

    /*
     * Starts a new synchronization attempt.
     *
     * The caller supplies a fresh synchronization token.
     * The session owns the link sequence number and remembers
     * both values so LINK_SYNC_OK can later be correlated.
     */
    [[nodiscard]] ProtocolFrame begin_synchronization(
        std::uint64_t sync_token);

    /*
     * Accepts LINK_SYNC_OK only when it matches the currently
     * pending sequence and synchronization token.
     *
     * Returns true only when synchronization succeeds.
     */
    [[nodiscard]] bool handle_link_sync_ok(const ProtocolFrame& frame, std::uint32_t now_ms);

    [[nodiscard]] std::optional<ProtocolFrame> heartbeat_if_due(std::uint32_t now_ms);

    [[nodiscard]] bool handle_heartbeat_response(const ProtocolFrame& frame, std::uint32_t now_ms);

    void disconnect();

    void check_link_timeout(std::uint32_t now_ms);

private:
    Stm32LinkState state_ =  Stm32LinkState::Disconnected;

    std::uint16_t next_sequence_ = 1U;

    std::uint16_t pending_sync_sequence_ = 0U;
    std::uint64_t pending_sync_token_ = 0U;

    std::uint16_t pending_heartbeat_sequence_ = 0U;
    std::uint32_t last_valid_response_ms_ = 0U;

    static constexpr std::uint32_t heartbeat_period_ms_ = 250U;
    static constexpr std::uint32_t link_timeout_ms_ = 1000U;
    std::uint32_t last_heartbeat_tx_ms_ = 0U;
    bool heartbeat_response_pending_ = false;
};