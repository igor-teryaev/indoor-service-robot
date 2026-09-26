#include <gtest/gtest.h>

#include <cstdint>

extern "C"
{
#include "motion_command_guard.h"
#include "uart_link_manager.h"
#include "uart_protocol_transmitter.h"
#include "link_sync_codec.h"
#include "heartbeat_codec.h"
#include "protocol_ingress_router.h"
}

namespace
{
    constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 1000U;
    bool stop_result = true;
    uint32_t stop_call_count = 0U;

    bool transmit_result = true;
    uint32_t transmit_call_count = 0U;
    ProtocolFrame transmitted_frame = {};

    void expect_transmitted_heartbeat(
    uint16_t expected_sequence,
    LinkState expected_link_state,
    uint32_t expected_uptime_ms)
    {
        ASSERT_EQ(transmit_call_count, 1U);

        EXPECT_EQ(
            transmitted_frame.message_type,
            PROTOCOL_MESSAGE_TYPE_HEARTBEAT);
        EXPECT_EQ(
            transmitted_frame.sequence,
            expected_sequence);
        EXPECT_EQ(
            transmitted_frame.payload_length,
            HEARTBEAT_WIRE_SIZE);

        HeartbeatPayload payload = {0};
        heartbeat_decode(
            transmitted_frame.payload,
            &payload);

        EXPECT_EQ(
            payload.link_state,
            expected_link_state);
        EXPECT_EQ(
            payload.uptime_ms,
            expected_uptime_ms);
    }
}

extern "C" bool motion_command_guard_stop(void)
{
    ++stop_call_count;
    return stop_result;
}

extern "C" bool uart_protocol_transmitter_send(
    const ProtocolFrame *frame)
{
    ++transmit_call_count;

    if (frame != nullptr)
    {
        transmitted_frame = *frame;
    }

    return transmit_result;
}

class UartLinkManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        stop_result = true;
        stop_call_count = 0U;

        transmit_result = true;
        transmit_call_count = 0U;
        transmitted_frame = {};

        ASSERT_TRUE(uart_link_manager_init(HEARTBEAT_TIMEOUT_MS));
    }
};

TEST_F(UartLinkManagerTest, StartsUnsynchronized)
{
    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 0U);
    EXPECT_EQ(transmit_call_count, 0U);
}

TEST_F(UartLinkManagerTest, StopsMotionAndEchoesValidLinkSync)
{
    constexpr LinkSyncPayload input_payload = {
        .sync_token = UINT64_C(0x0123456789ABCDEF)
    };

    ProtocolFrame input_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 0x1234U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &input_payload,
        input_frame.payload);

    const UartLinkManagerResult result =
        uart_link_manager_handle(&input_frame, 100U);

    EXPECT_EQ(
        result,
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    EXPECT_EQ(stop_call_count, 1U);
    EXPECT_EQ(transmit_call_count, 1U);
    EXPECT_TRUE(uart_link_manager_is_synchronized());

    EXPECT_EQ(
        transmitted_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_LINK_SYNC_OK);
    EXPECT_EQ(
        transmitted_frame.sequence,
        input_frame.sequence);
    EXPECT_EQ(
        transmitted_frame.payload_length,
        LINK_SYNC_WIRE_SIZE);

    LinkSyncPayload output_payload = {0};
    link_sync_decode(
        transmitted_frame.payload,
        &output_payload);

    EXPECT_EQ(
        output_payload.sync_token,
        input_payload.sync_token);
}

TEST_F(UartLinkManagerTest, RejectsNullFrameWithoutSideEffects)
{
    EXPECT_EQ(
        uart_link_manager_handle(nullptr, 100U),
        UART_LINK_MANAGER_RESULT_INVALID_ARGUMENT);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 0U);
    EXPECT_EQ(transmit_call_count, 0U);
}

TEST_F(UartLinkManagerTest, IgnoresNonLinkMessage)
{
    constexpr ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY,
        .sequence = 5U,
        .payload_length = 0U
    };

    EXPECT_EQ(
        uart_link_manager_handle(&frame, 100U), UART_LINK_MANAGER_RESULT_NOT_LINK_MESSAGE);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 0U);
    EXPECT_EQ(transmit_call_count, 0U);
}

TEST_F(UartLinkManagerTest, InvalidLinkSyncStopsMotionAndClearsSynchronization)
{
    constexpr LinkSyncPayload payload = {
        .sync_token = UINT64_C(0x1122334455667788)
    };

    ProtocolFrame valid_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 10U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(&payload, valid_frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&valid_frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);
    ASSERT_TRUE(uart_link_manager_is_synchronized());

    stop_call_count = 0U;
    transmit_call_count = 0U;

    ProtocolFrame invalid_frame = valid_frame;
    invalid_frame.payload_length = LINK_SYNC_WIRE_SIZE - 1U;

    EXPECT_EQ(
        uart_link_manager_handle(&invalid_frame, 100U),
        UART_LINK_MANAGER_RESULT_INVALID_LINK_SYNC);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);
    EXPECT_EQ(transmit_call_count, 0U);
}

TEST_F(UartLinkManagerTest, StopFailureClearsSynchronizationAndSuppressesResponse)
{
    constexpr LinkSyncPayload payload = {
        .sync_token = UINT64_C(0x0102030405060708)
    };

    ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 11U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(&payload, frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);
    ASSERT_TRUE(uart_link_manager_is_synchronized());

    stop_call_count = 0U;
    transmit_call_count = 0U;
    stop_result = false;

    EXPECT_EQ(
        uart_link_manager_handle(&frame, 100U),
        UART_LINK_MANAGER_RESULT_STOP_FAILED);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);
    EXPECT_EQ(transmit_call_count, 0U);
}

TEST_F(UartLinkManagerTest, TransmitFailureLeavesLinkUnsynchronized)
{
    constexpr LinkSyncPayload payload = {
        .sync_token = UINT64_C(0x8877665544332211)
    };

    ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 12U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(&payload, frame.payload);

    transmit_result = false;

    EXPECT_EQ(
        uart_link_manager_handle(&frame, 100U),
        UART_LINK_MANAGER_RESULT_TRANSMIT_FAILED);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);
    EXPECT_EQ(transmit_call_count, 1U);
}

TEST_F(UartLinkManagerTest, AcceptsHeartbeatAfterSynchronization)
{
    constexpr LinkSyncPayload sync_payload = {
        .sync_token = UINT64_C(0x123456789ABCDEF0)
    };

    ProtocolFrame sync_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 20U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&sync_frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    stop_call_count = 0U;
    transmit_call_count = 0U;

    constexpr HeartbeatPayload heartbeat_payload = {
        .link_state = LINK_STATE_SYNCHRONIZED,
        .uptime_ms = 5000U
    };

    ProtocolFrame heartbeat_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 21U,
        .payload_length = HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_frame.payload);

    EXPECT_EQ(
        uart_link_manager_handle(
            &heartbeat_frame,
            900U),
        UART_LINK_MANAGER_RESULT_HEARTBEAT_ACCEPTED);

    EXPECT_TRUE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 0U);

    expect_transmitted_heartbeat(
        21U,
        LINK_STATE_SYNCHRONIZED,
        900U);
}

TEST_F(UartLinkManagerTest, RejectsHeartbeatBeforeSynchronization)
{
    constexpr HeartbeatPayload payload = {
        .link_state = LINK_STATE_SYNCHRONIZED,
        .uptime_ms = 100U
    };

    ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 30U,
        .payload_length = HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(&payload, frame.payload);

    EXPECT_EQ(
        uart_link_manager_handle(&frame, 100U),
        UART_LINK_MANAGER_RESULT_NOT_SYNCHRONIZED);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 0U);

    expect_transmitted_heartbeat(
        30U,
        LINK_STATE_UNSYNCHRONIZED,
        100U);
}

TEST_F(UartLinkManagerTest, PeerUnsynchronizedHeartbeatStopsMotion)
{
    constexpr LinkSyncPayload sync_payload = {
        .sync_token = UINT64_C(0x1020304050607080)
    };

    ProtocolFrame sync_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 40U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&sync_frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    stop_call_count = 0U;
    transmit_call_count = 0U;

    constexpr HeartbeatPayload heartbeat_payload = {
        .link_state = LINK_STATE_UNSYNCHRONIZED,
        .uptime_ms = 200U
    };

    ProtocolFrame heartbeat_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 41U,
        .payload_length = HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_frame.payload);

    EXPECT_EQ(
        uart_link_manager_handle(
            &heartbeat_frame,
            200U),
        UART_LINK_MANAGER_RESULT_PEER_UNSYNCHRONIZED);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);

    expect_transmitted_heartbeat(
        41U,
        LINK_STATE_UNSYNCHRONIZED,
        200U);
}

TEST_F(UartLinkManagerTest, InvalidHeartbeatLengthStopsMotion)
{
    constexpr LinkSyncPayload sync_payload = {
        .sync_token = UINT64_C(0x0101010101010101)
    };

    ProtocolFrame sync_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 50U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&sync_frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    stop_call_count = 0U;
    transmit_call_count = 0U;

    ProtocolFrame heartbeat_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 51U,
        .payload_length = HEARTBEAT_WIRE_SIZE - 1U
    };

    EXPECT_EQ(
        uart_link_manager_handle(
            &heartbeat_frame,
            200U),
        UART_LINK_MANAGER_RESULT_INVALID_HEARTBEAT);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);
    EXPECT_EQ(transmit_call_count, 0U);
}

TEST_F(UartLinkManagerTest, InvalidHeartbeatLinkStateStopsMotion)
{
    constexpr LinkSyncPayload sync_payload = {
        .sync_token = UINT64_C(0x0202020202020202)
    };

    ProtocolFrame sync_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 60U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&sync_frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    stop_call_count = 0U;
    transmit_call_count = 0U;

    constexpr HeartbeatPayload invalid_payload = {
        .link_state = 0x7FU,
        .uptime_ms = 300U
    };

    ProtocolFrame heartbeat_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 61U,
        .payload_length = HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &invalid_payload,
        heartbeat_frame.payload);

    EXPECT_EQ(
        uart_link_manager_handle(
            &heartbeat_frame,
            300U),
        UART_LINK_MANAGER_RESULT_INVALID_HEARTBEAT);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);
    EXPECT_EQ(transmit_call_count, 0U);
}

TEST_F(UartLinkManagerTest, StopsMotionAtHeartbeatTimeoutBoundary)
{
    constexpr LinkSyncPayload payload = {
        .sync_token = UINT64_C(0x0303030303030303)
    };

    ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 70U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(&payload, frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    stop_call_count = 0U;
    transmit_call_count = 0U;

    EXPECT_EQ(
        uart_link_manager_update(1099U),
        UART_LINK_MANAGER_UPDATE_NONE);
    EXPECT_TRUE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 0U);

    EXPECT_EQ(
        uart_link_manager_update(1100U),
        UART_LINK_MANAGER_UPDATE_LINK_LOST);
    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);

    EXPECT_EQ(
        uart_link_manager_update(1101U),
        UART_LINK_MANAGER_UPDATE_NONE);
    EXPECT_EQ(stop_call_count, 1U);
}

TEST_F(UartLinkManagerTest, HeartbeatRestartsLinkTimeout)
{
    constexpr LinkSyncPayload sync_payload = {
        .sync_token = UINT64_C(0x0404040404040404)
    };

    ProtocolFrame sync_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 80U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&sync_frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    constexpr HeartbeatPayload heartbeat_payload = {
        .link_state = LINK_STATE_SYNCHRONIZED,
        .uptime_ms = 800U
    };

    ProtocolFrame heartbeat_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 81U,
        .payload_length = HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(
            &heartbeat_frame,
            900U),
        UART_LINK_MANAGER_RESULT_HEARTBEAT_ACCEPTED);

    stop_call_count = 0U;
    transmit_call_count = 0U;

    EXPECT_EQ(
        uart_link_manager_update(1899U),
        UART_LINK_MANAGER_UPDATE_NONE);
    EXPECT_TRUE(uart_link_manager_is_synchronized());

    EXPECT_EQ(
        uart_link_manager_update(1900U),
        UART_LINK_MANAGER_UPDATE_LINK_LOST);
    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);
}

TEST_F(UartLinkManagerTest, StopFailureAtTimeoutStillClearsSynchronization)
{
    constexpr LinkSyncPayload payload = {
        .sync_token = UINT64_C(0x0505050505050505)
    };

    ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 90U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(&payload, frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    stop_call_count = 0U;
    transmit_call_count = 0U;
    stop_result = false;

    EXPECT_EQ(
        uart_link_manager_update(1100U),
        UART_LINK_MANAGER_UPDATE_STOP_FAILED);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);

    EXPECT_EQ(
        uart_link_manager_update(1101U),
        UART_LINK_MANAGER_UPDATE_NONE);
    EXPECT_EQ(stop_call_count, 1U);
}

TEST_F(UartLinkManagerTest, HeartbeatTimeoutWorksAcrossTickWraparound)
{
    constexpr uint32_t start_ms =
        UINT32_MAX - 499U;

    constexpr LinkSyncPayload payload = {
        .sync_token = UINT64_C(0x0606060606060606)
    };

    ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 100U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(&payload, frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(
            &frame,
            start_ms),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    stop_call_count = 0U;
    transmit_call_count = 0U;

    EXPECT_EQ(
        uart_link_manager_update(499U),
        UART_LINK_MANAGER_UPDATE_NONE);
    EXPECT_TRUE(uart_link_manager_is_synchronized());

    EXPECT_EQ(
        uart_link_manager_update(500U),
        UART_LINK_MANAGER_UPDATE_LINK_LOST);
    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(stop_call_count, 1U);
}

TEST_F(UartLinkManagerTest, InvalidReinitializationClearsActiveLink)
{
    constexpr LinkSyncPayload payload = {
        .sync_token = UINT64_C(0x0707070707070707)
    };

    ProtocolFrame frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 110U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(&payload, frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);
    ASSERT_TRUE(uart_link_manager_is_synchronized());

    EXPECT_FALSE(uart_link_manager_init(0U));
    EXPECT_FALSE(uart_link_manager_is_synchronized());

    EXPECT_EQ(
        uart_link_manager_handle(&frame, 200U),
        UART_LINK_MANAGER_RESULT_NOT_INITIALIZED);

    EXPECT_EQ(
        uart_link_manager_update(5000U),
        UART_LINK_MANAGER_UPDATE_NONE);
}

TEST_F(UartLinkManagerTest, HeartbeatTransmitFailureClearsLinkAndStopsMotion)
{
    constexpr LinkSyncPayload sync_payload = {
        .sync_token = UINT64_C(0x0808080808080808)
    };

    ProtocolFrame sync_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_LINK_SYNC,
        .sequence = 120U,
        .payload_length = LINK_SYNC_WIRE_SIZE
    };

    link_sync_encode(
        &sync_payload,
        sync_frame.payload);

    ASSERT_EQ(
        uart_link_manager_handle(&sync_frame, 100U),
        UART_LINK_MANAGER_RESULT_SYNCHRONIZED);

    stop_call_count = 0U;
    transmit_call_count = 0U;
    transmit_result = false;

    constexpr HeartbeatPayload heartbeat_payload = {
        .link_state = LINK_STATE_SYNCHRONIZED,
        .uptime_ms = 800U
    };

    ProtocolFrame heartbeat_frame = {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 121U,
        .payload_length = HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &heartbeat_payload,
        heartbeat_frame.payload);

    EXPECT_EQ(
        uart_link_manager_handle(
            &heartbeat_frame,
            900U),
        UART_LINK_MANAGER_RESULT_TRANSMIT_FAILED);

    EXPECT_FALSE(uart_link_manager_is_synchronized());
    EXPECT_EQ(transmit_call_count, 1U);
    EXPECT_EQ(stop_call_count, 1U);
}

TEST_F(
    UartLinkManagerTest,
    MalformedHeartbeatStillBelongsToLinkManager)
{
    constexpr HeartbeatPayload payload =
    {
        .link_state = LINK_STATE_SYNCHRONIZED,
        .uptime_ms = 100U
    };

    ProtocolFrame frame =
    {
        .message_type = PROTOCOL_MESSAGE_TYPE_HEARTBEAT,
        .sequence = 130U,
        .payload_length = HEARTBEAT_WIRE_SIZE
    };

    heartbeat_encode(
        &payload,
        frame.payload);

    /*
     * Corrupt only the payload length.
     *
     * The frame is still semantically a HEARTBEAT message
     * and must therefore be dispatched to uart_link_manager.
     */
    frame.payload_length =
        HEARTBEAT_WIRE_SIZE - 1U;

    EXPECT_EQ(
        frame.message_type,
        PROTOCOL_MESSAGE_TYPE_HEARTBEAT);

    EXPECT_EQ(
        protocol_ingress_route(&frame),
        PROTOCOL_INGRESS_ROUTE_INVALID_PAYLOAD_LENGTH);
}
