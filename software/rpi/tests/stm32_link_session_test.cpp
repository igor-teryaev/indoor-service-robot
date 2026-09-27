#include <gtest/gtest.h>

#include "stm32_link_session.h"

extern "C"
{
#include "link_sync_codec.h"
#include "protocol_message_type.h"
#include "heartbeat_codec.h"
#include "protocol_frame_encoder.h"
#include "protocol_frame_decoder.h"
}

TEST(Stm32LinkSessionTest, StartsDisconnected)
{
    const Stm32LinkSession session;

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Disconnected
    );
}

TEST(Stm32LinkSessionTest, BeginSynchronizationProducesLinkSyncFrame)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame frame =
        session.begin_synchronization(
            sync_token);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::SyncPending
    );

    EXPECT_EQ(
        frame.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC
    );

    EXPECT_EQ(
        frame.sequence,
        1U
    );

    EXPECT_EQ(
        frame.payload_length,
        LINK_SYNC_WIRE_SIZE
    );

    LinkSyncPayload payload = {};

    link_sync_decode(
        frame.payload,
        &payload);

    EXPECT_EQ(
        payload.sync_token,
        sync_token
    );
}

TEST(
    Stm32LinkSessionTest,
    RejectsLinkSyncOkWithWrongSequence)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            static_cast<std::uint16_t>(
                request.sequence + 1U),

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &payload,
        response.payload);

    EXPECT_FALSE(
        session.handle_link_sync_ok(response, 100U)
        );

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::SyncPending
    );
}

TEST(
    Stm32LinkSessionTest,
    RejectsLinkSyncOkWithWrongToken)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload payload =
    {
        .sync_token =
            UINT64_C(0xFEDCBA9876543210)
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &payload,
        response.payload);

    EXPECT_FALSE(
        session.handle_link_sync_ok(response, 100U)
    );

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::SyncPending
    );
}

TEST(
    Stm32LinkSessionTest,
    AcceptsMatchingLinkSyncOk)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &payload,
        response.payload);

    EXPECT_TRUE(
        session.handle_link_sync_ok(response, 100U)
    );

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Synchronized
    );
}

TEST(
    Stm32LinkSessionTest,
    RejectsLinkSyncOkWithWrongMessageType)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame request =
        session.begin_synchronization(sync_token);

    const LinkSyncPayload payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC,

        .sequence = request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &payload,
        response.payload);

    EXPECT_FALSE(
        session.handle_link_sync_ok(response, 100U));

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::SyncPending);
}

TEST(
    Stm32LinkSessionTest,
    RejectsLinkSyncOkWithWrongPayloadLength)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame request =
        session.begin_synchronization(sync_token);

    const LinkSyncPayload payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE - 1U
    };

    link_sync_encode(
        &payload,
        response.payload);

    EXPECT_FALSE(
        session.handle_link_sync_ok(response, 100U));

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::SyncPending);
}

TEST(
    Stm32LinkSessionTest,
    DoesNotSendHeartbeatBeforePeriodExpires)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &payload,
        response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            response,
            1000U)
    );

    const auto heartbeat =
        session.heartbeat_if_due(
            1249U);

    EXPECT_FALSE(
        heartbeat.has_value()
    );
}

TEST(
    Stm32LinkSessionTest,
    SendsHeartbeatAtPeriodBoundary)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U)
    );

    const auto heartbeat =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        heartbeat.has_value()
    );

    EXPECT_EQ(
        heartbeat->message_type,
        PROTOCOL_MESSAGE_TYPE_HEARTBEAT
    );

    EXPECT_EQ(
        heartbeat->sequence,
        2U
    );

    EXPECT_EQ(
        heartbeat->payload_length,
        HEARTBEAT_WIRE_SIZE
    );

    HeartbeatPayload payload = {};

    heartbeat_decode(
        heartbeat->payload,
        &payload);

    EXPECT_EQ(
        payload.link_state,
        LINK_STATE_SYNCHRONIZED
    );

    EXPECT_EQ(
        payload.uptime_ms,
        1250U
    );
}

TEST(
    Stm32LinkSessionTest,
    WaitsAnotherFullPeriodAfterSendingHeartbeat)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U)
    );

    const auto first_heartbeat =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        first_heartbeat.has_value()
    );

    EXPECT_FALSE(
        session.heartbeat_if_due(
            1499U).has_value()
    );

    const auto second_heartbeat =
        session.heartbeat_if_due(
            1500U);

    ASSERT_TRUE(
        second_heartbeat.has_value()
    );

    EXPECT_EQ(
        second_heartbeat->sequence,
        3U
    );
}

TEST(
    Stm32LinkSessionTest,
    AcceptsMatchingSynchronizedHeartbeatResponse)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U)
    );

    const auto heartbeat_request =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        heartbeat_request.has_value()
    );

    const HeartbeatPayload heartbeat_payload =
    {
        .link_state =
            LINK_STATE_SYNCHRONIZED,

        .uptime_ms = 5000U
    };

    ProtocolFrame heartbeat_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            heartbeat_request->sequence,

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_response.payload);

    EXPECT_TRUE(
        session.handle_heartbeat_response(
            heartbeat_response,
            1260U)
    );

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Synchronized
    );
}

TEST(
    Stm32LinkSessionTest,
    RejectsHeartbeatResponseWithWrongSequence)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U));

    const auto heartbeat_request =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        heartbeat_request.has_value());

    const HeartbeatPayload heartbeat_payload =
    {
        .link_state =
            LINK_STATE_SYNCHRONIZED,

        .uptime_ms = 5000U
    };

    ProtocolFrame heartbeat_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            static_cast<std::uint16_t>(
                heartbeat_request->sequence + 1U),

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_response.payload);

    EXPECT_FALSE(
        session.handle_heartbeat_response(
            heartbeat_response,
            1260U));

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Synchronized);
}

TEST(
    Stm32LinkSessionTest,
    UnsynchronizedHeartbeatResponseClearsSynchronization)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U));

    const auto heartbeat_request =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        heartbeat_request.has_value());

    const HeartbeatPayload heartbeat_payload =
    {
        .link_state =
            LINK_STATE_UNSYNCHRONIZED,

        .uptime_ms = 5000U
    };

    ProtocolFrame heartbeat_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            heartbeat_request->sequence,

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_response.payload);

    EXPECT_FALSE(
        session.handle_heartbeat_response(
            heartbeat_response,
            1260U));

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Disconnected);
}

TEST(
    Stm32LinkSessionTest,
    DisconnectsWhenLinkResponseTimeoutExpires)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U));

    session.check_link_timeout(
        1999U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Synchronized);

    session.check_link_timeout(
        2000U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Disconnected);
}

TEST(
    Stm32LinkSessionTest,
    ValidHeartbeatResponseRefreshesLinkTimeout)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U));

    const auto heartbeat_request =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        heartbeat_request.has_value());

    const HeartbeatPayload heartbeat_payload =
    {
        .link_state =
            LINK_STATE_SYNCHRONIZED,

        .uptime_ms = 5000U
    };

    ProtocolFrame heartbeat_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            heartbeat_request->sequence,

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_response.payload);

    ASSERT_TRUE(
        session.handle_heartbeat_response(
            heartbeat_response,
            1260U));

    session.check_link_timeout(
        2259U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Synchronized);

    session.check_link_timeout(
        2260U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Disconnected);
}

TEST(
    Stm32LinkSessionTest,
    RejectedHeartbeatResponseDoesNotRefreshLinkTimeout)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U));

    const auto heartbeat_request =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        heartbeat_request.has_value());

    const HeartbeatPayload heartbeat_payload =
    {
        .link_state =
            LINK_STATE_SYNCHRONIZED,

        .uptime_ms = 5000U
    };

    ProtocolFrame heartbeat_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            static_cast<std::uint16_t>(
                heartbeat_request->sequence + 1U),

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_response.payload);

    EXPECT_FALSE(
        session.handle_heartbeat_response(
            heartbeat_response,
            1260U));

    session.check_link_timeout(
        1999U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Synchronized);

    session.check_link_timeout(
        2000U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Disconnected);
}

TEST(
    Stm32LinkSessionTest,
    SendingHeartbeatsDoesNotRefreshLinkTimeout)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U));

    ASSERT_TRUE(
        session.heartbeat_if_due(
            1250U).has_value());

    ASSERT_TRUE(
        session.heartbeat_if_due(
            1500U).has_value());

    ASSERT_TRUE(
        session.heartbeat_if_due(
            1750U).has_value());

    session.check_link_timeout(
        1999U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Synchronized);

    session.check_link_timeout(
        2000U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Disconnected);
}

TEST(
    Stm32LinkSessionTest,
    RejectsDuplicateHeartbeatResponse)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U));

    const auto heartbeat_request =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        heartbeat_request.has_value());

    const HeartbeatPayload heartbeat_payload =
    {
        .link_state =
            LINK_STATE_SYNCHRONIZED,

        .uptime_ms = 5000U
    };

    ProtocolFrame heartbeat_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            heartbeat_request->sequence,

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_response.payload);

    ASSERT_TRUE(
        session.handle_heartbeat_response(
            heartbeat_response,
            1260U));

    EXPECT_FALSE(
        session.handle_heartbeat_response(
            heartbeat_response,
            1500U));

    session.check_link_timeout(
        2260U);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Disconnected);
}

TEST(
    Stm32LinkSessionTest,
    NewSynchronizationAttemptInvalidatesPreviousAttempt)
{
    constexpr std::uint64_t first_token =
        UINT64_C(0x0123456789ABCDEF);

    constexpr std::uint64_t second_token =
        UINT64_C(0xFEDCBA9876543210);

    Stm32LinkSession session;

    const ProtocolFrame first_request =
        session.begin_synchronization(
            first_token);

    const ProtocolFrame second_request =
        session.begin_synchronization(
            second_token);

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::SyncPending);

    const LinkSyncPayload old_payload =
    {
        .sync_token = first_token
    };

    ProtocolFrame old_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            first_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &old_payload,
        old_response.payload);

    EXPECT_FALSE(
        session.handle_link_sync_ok(
            old_response,
            1000U));

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::SyncPending);

    const LinkSyncPayload current_payload =
    {
        .sync_token = second_token
    };

    ProtocolFrame current_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            second_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &current_payload,
        current_response.payload);

    EXPECT_TRUE(
        session.handle_link_sync_ok(
            current_response,
            1010U));

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Synchronized);
}

TEST(
    Stm32LinkSessionTest,
    NewSynchronizationAttemptInvalidatesPendingHeartbeat)
{
    constexpr std::uint64_t first_token =
        UINT64_C(0x0123456789ABCDEF);

    constexpr std::uint64_t second_token =
        UINT64_C(0xFEDCBA9876543210);

    Stm32LinkSession session;

    const ProtocolFrame first_sync_request =
        session.begin_synchronization(
            first_token);

    const LinkSyncPayload first_sync_payload =
    {
        .sync_token = first_token
    };

    ProtocolFrame first_sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            first_sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &first_sync_payload,
        first_sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            first_sync_response,
            1000U));

    const auto old_heartbeat_request =
        session.heartbeat_if_due(
            1250U);

    ASSERT_TRUE(
        old_heartbeat_request.has_value());

    session.check_link_timeout(
        2000U);

    ASSERT_EQ(
        session.state(),
        Stm32LinkState::Disconnected);

    const ProtocolFrame second_sync_request =
        session.begin_synchronization(
            second_token);

    const LinkSyncPayload second_sync_payload =
    {
        .sync_token = second_token
    };

    ProtocolFrame second_sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            second_sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &second_sync_payload,
        second_sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            second_sync_response,
            2010U));

    const HeartbeatPayload old_heartbeat_payload =
    {
        .link_state =
            LINK_STATE_SYNCHRONIZED,

        .uptime_ms = 5000U
    };

    ProtocolFrame old_heartbeat_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT,

        .sequence =
            old_heartbeat_request->sequence,

        .payload_length =
            HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &old_heartbeat_payload,
        old_heartbeat_response.payload);

    EXPECT_FALSE(
        session.handle_heartbeat_response(
            old_heartbeat_response,
            2020U));
}

TEST(
    Stm32LinkSessionTest,
    TransportDisconnectClearsLinkState)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame sync_request =
        session.begin_synchronization(
            sync_token);

    const LinkSyncPayload sync_payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame sync_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence =
            sync_request.sequence,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_response.payload);

    ASSERT_TRUE(
        session.handle_link_sync_ok(
            sync_response,
            1000U));

    ASSERT_EQ(
        session.state(),
        Stm32LinkState::Synchronized);

    session.disconnect();

    EXPECT_EQ(
        session.state(),
        Stm32LinkState::Disconnected);

    EXPECT_FALSE(
        session.heartbeat_if_due(
            5000U).has_value());
}

TEST(
    Stm32LinkSessionTest,
    SynchronizationFrameCanBeEncodedForSerialTransmission)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    Stm32LinkSession session;

    const ProtocolFrame frame =
        session.begin_synchronization(
            sync_token);

    std::uint8_t wire_data[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t wire_size =
        protocol_frame_encode(
            &frame,
            wire_data);

    EXPECT_GT(
        wire_size,
        0U);

    EXPECT_LE(
        wire_size,
        PROTOCOL_FRAME_MAX_WIRE_SIZE);

    EXPECT_EQ(
        wire_data[PROTOCOL_FRAME_MAGIC_0_OFFSET],
        PROTOCOL_FRAME_MAGIC_0);

    EXPECT_EQ(
        wire_data[PROTOCOL_FRAME_MAGIC_1_OFFSET],
        PROTOCOL_FRAME_MAGIC_1);
}

TEST(
    Stm32LinkSessionTest,
    SerialBytesCanBeDecodedIntoProtocolFrame)
{
    constexpr std::uint64_t sync_token =
        UINT64_C(0x0123456789ABCDEF);

    const LinkSyncPayload payload =
    {
        .sync_token = sync_token
    };

    ProtocolFrame source_frame =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK,

        .sequence = 42U,

        .payload_length =
            LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &payload,
        source_frame.payload);

    std::uint8_t wire_data[
        PROTOCOL_FRAME_MAX_WIRE_SIZE] = {};

    const std::size_t wire_size =
        protocol_frame_encode(
            &source_frame,
            wire_data);

    ASSERT_GT(
        wire_size,
        0U);

    ProtocolFrameDecoder decoder = {};

    protocol_frame_decoder_init(
        &decoder);

    const ProtocolFrame* decoded_frame =
        nullptr;

    for (std::size_t i = 0U;
         i < wire_size;
         ++i)
    {
        const bool completed =
            protocol_frame_decoder_feed_byte(
                &decoder,
                wire_data[i],
                &decoded_frame);

        if (i + 1U < wire_size)
        {
            EXPECT_FALSE(completed);
        }
        else
        {
            EXPECT_TRUE(completed);
        }
    }

    ASSERT_NE(
        decoded_frame,
        nullptr);

    EXPECT_EQ(
        decoded_frame->message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK);

    EXPECT_EQ(
        decoded_frame->sequence,
        42U);

    EXPECT_EQ(
        decoded_frame->payload_length,
        LINK_SYNC_WIRE_SIZE);
}