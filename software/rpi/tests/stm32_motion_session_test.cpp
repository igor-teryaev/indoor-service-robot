#include <gtest/gtest.h>

#include "stm32_motion_session.h"

#include <cstring>

extern "C"
{
#include "motion_lifecycle_command_codec.h"
#include "motion_lifecycle_command_type.h"
#include "protocol_message_type.h"
#include "motion_ack_codec.h"
#include "motion_response_codec.h"
#include "wheel_velocity_payload_codec.h"
}

TEST(
    Stm32MotionSessionTest,
    BeginStartSessionProducesMotionCommandFrame)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(0x12345678);

    Stm32MotionSession session;

    const auto frame =
        session.begin_start_session(
            motion_session_id);

    ASSERT_TRUE(frame.has_value());

    EXPECT_EQ(
        frame->message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND
    );

    EXPECT_EQ(frame->sequence, 1U);

    EXPECT_EQ(
        frame->payload_length,
        MOTION_LIFECYCLE_COMMAND_WIRE_SIZE
    );

    MotionLifecycleCommandPayload payload = {};

    motion_lifecycle_command_decode(
        frame->payload,
        &payload);

    EXPECT_EQ(
        payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION
    );

    EXPECT_EQ(
        payload.motion_session_id,
        motion_session_id
    );
}

TEST(
    Stm32MotionSessionTest,
    AcceptsAckForPendingStartSession)
{
    Stm32MotionSession session;

    const auto request =
        session.begin_start_session(
            UINT32_C(0x12345678));

    const MotionAckPayload payload =
    {
        .status = MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            request->sequence,

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &payload,
        ack.payload);

    EXPECT_TRUE(
        session.handle_ack(ack, 1050U)
    );
}

TEST(
    Stm32MotionSessionTest,
    RejectsAckWithWrongSequence)
{
    Stm32MotionSession session;

    const auto request =
        session.begin_start_session(
            UINT32_C(0x12345678));

    const MotionAckPayload payload =
    {
        .status = MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            static_cast<std::uint16_t>(
                request->sequence + 1U),

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &payload,
        ack.payload);

    EXPECT_FALSE(
        session.handle_ack(ack, 1050U)
    );
}

TEST(
    Stm32MotionSessionTest,
    AcceptsMatchingResponseAndReturnsResult)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(0x12345678);

    Stm32MotionSession session;

    const auto request =
        session.begin_start_session(
            motion_session_id);

    const MotionResponsePayload payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_ALREADY_ACTIVE
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            request->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &payload,
        response.payload);

    const auto result =
        session.handle_response(response);

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result.value(),
        MOTION_RESPONSE_ALREADY_ACTIVE
    );

    // Terminal response completes the transaction.
    EXPECT_FALSE(
        session.handle_response(response).has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    RejectsResponseWithWrongSequence)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(0x12345678);

    Stm32MotionSession session;

    const auto request =
        session.begin_start_session(
            motion_session_id);

    const MotionResponsePayload payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            static_cast<std::uint16_t>(
                request->sequence + 1U),

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &payload,
        response.payload);

    EXPECT_FALSE(
        session.handle_response(response).has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    RejectsResponseWithWrongSessionId)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(0x12345678);

    Stm32MotionSession session;

    const auto request =
        session.begin_start_session(
            motion_session_id);

    const MotionResponsePayload payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            UINT32_C(0x87654321),

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            request->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &payload,
        response.payload);

    EXPECT_FALSE(
        session.handle_response(response).has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    RejectsStartWhileTransactionIsPending)
{
    Stm32MotionSession session;

    const auto first =
        session.begin_start_session(
            UINT32_C(100));

    ASSERT_TRUE(first.has_value());

    const auto second =
        session.begin_start_session(
            UINT32_C(200));

    EXPECT_FALSE(second.has_value());
}

TEST(
    Stm32MotionSessionTest,
    AllowsNewStartAfterTerminalResponse)
{
    constexpr std::uint32_t first_session_id =
        UINT32_C(100);

    Stm32MotionSession session;

    const auto first =
        session.begin_start_session(
            first_session_id);

    ASSERT_TRUE(first.has_value());

    const MotionResponsePayload payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            first_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            first->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &payload,
        response.payload);

    ASSERT_TRUE(
        session.handle_response(response).has_value()
    );

    const auto second =
        session.begin_start_session(
            UINT32_C(200));

    ASSERT_TRUE(second.has_value());

    EXPECT_EQ(
        second->sequence,
        2U
    );
}

TEST(
    Stm32MotionSessionTest,
    BeginEndSessionProducesMotionCommandFrame)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(0x12345678);

    Stm32MotionSession session;

    const auto frame =
        session.begin_end_session(
            motion_session_id);

    ASSERT_TRUE(frame.has_value());

    EXPECT_EQ(
        frame->message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND
    );

    EXPECT_EQ(
        frame->sequence,
        1U
    );

    EXPECT_EQ(
        frame->payload_length,
        MOTION_LIFECYCLE_COMMAND_WIRE_SIZE
    );

    MotionLifecycleCommandPayload payload = {};

    motion_lifecycle_command_decode(
        frame->payload,
        &payload);

    EXPECT_EQ(
        payload.command,
        MOTION_LIFECYCLE_COMMAND_END_SESSION
    );

    EXPECT_EQ(
        payload.motion_session_id,
        motion_session_id
    );
}

TEST(
    Stm32MotionSessionTest,
    AcceptsMatchingEndSessionResponse)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(0x12345678);

    Stm32MotionSession session;

    const auto request =
        session.begin_end_session(
            motion_session_id);

    ASSERT_TRUE(request.has_value());

    const MotionResponsePayload payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_END_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_ALREADY_ENDED
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            request->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &payload,
        response.payload);

    const auto result =
        session.handle_response(response);

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result.value(),
        MOTION_RESPONSE_ALREADY_ENDED
    );
}

TEST(
    Stm32MotionSessionTest,
    RejectsResponseWithWrongCommand)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(0x12345678);

    Stm32MotionSession session;

    const auto request =
        session.begin_end_session(
            motion_session_id);

    ASSERT_TRUE(request.has_value());

    const MotionResponsePayload payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            request->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &payload,
        response.payload);

    EXPECT_FALSE(
        session.handle_response(response).has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    RetryReusesPendingTransaction)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(0x12345678);

    Stm32MotionSession session;

    const auto original =
        session.begin_start_session(
            motion_session_id);

    ASSERT_TRUE(original.has_value());

    const auto retry =
        session.retry_pending_transaction();

    ASSERT_TRUE(retry.has_value());

    EXPECT_EQ(
        retry->message_type,
        original->message_type
    );

    EXPECT_EQ(
        retry->sequence,
        original->sequence
    );

    EXPECT_EQ(
        retry->payload_length,
        original->payload_length
    );

    EXPECT_EQ(
        0,
        std::memcmp(
            retry->payload,
            original->payload,
            original->payload_length)
    );
}

TEST(
    Stm32MotionSessionTest,
    NoRetryAfterTerminalResponse)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(100);

    Stm32MotionSession session;

    const auto request =
        session.begin_start_session(
            motion_session_id);

    ASSERT_TRUE(request.has_value());

    const MotionResponsePayload payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            request->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &payload,
        response.payload);

    ASSERT_TRUE(
        session.handle_response(response).has_value()
    );

    EXPECT_FALSE(
        session.retry_pending_transaction().has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    RetriesAfterAckTimeout)
{
    Stm32MotionSession session;

    const auto original =
        session.begin_start_session(
            UINT32_C(100));

    ASSERT_TRUE(original.has_value());

    session.mark_pending_transmitted(1000U);

    EXPECT_FALSE(
        session.retry_if_due(1099U).has_value()
    );

    const auto retry =
        session.retry_if_due(1100U);

    ASSERT_TRUE(retry.has_value());

    EXPECT_EQ(
        retry->sequence,
        original->sequence
    );
}

TEST(Stm32MotionSessionTest, WaitsForTerminalResponseAfterAck)
{
    Stm32MotionSession session;

    const auto request =
        session.begin_start_session(
            UINT32_C(100));

    ASSERT_TRUE(request.has_value());

    session.mark_pending_transmitted(1000U);

    const MotionAckPayload payload =
    {
        .status = MOTION_ACK_ACCEPTED
    };

    ProtocolFrame ack =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_ACK,

        .sequence =
            request->sequence,

        .payload_length =
            MOTION_ACK_WIRE_SIZE
    };

    motion_ack_encode(
        &payload,
        ack.payload);

    ASSERT_TRUE(
        session.handle_ack(ack, 1090U)
    );

    EXPECT_FALSE(
        session.retry_if_due(1839U).has_value()
    );

    EXPECT_TRUE(
        session.retry_if_due(1840U).has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    StopsRetryingAfterThreeRetransmissions)
{
    Stm32MotionSession session;

    ASSERT_TRUE(
        session.begin_start_session(
            UINT32_C(100)).has_value()
    );

    session.mark_pending_transmitted(0U);

    ASSERT_TRUE(
        session.retry_if_due(100U).has_value()
    );

    session.mark_pending_transmitted(100U);

    ASSERT_TRUE(
        session.retry_if_due(200U).has_value()
    );

    session.mark_pending_transmitted(200U);

    ASSERT_TRUE(
        session.retry_if_due(300U).has_value()
    );

    session.mark_pending_transmitted(300U);

    EXPECT_FALSE(
        session.retry_if_due(400U).has_value()
    );

    EXPECT_TRUE(
        session.retry_exhausted(400U)
    );
}

TEST(
    Stm32MotionSessionTest,
    ResetInvalidatesPendingTransaction)
{
    Stm32MotionSession session;

    ASSERT_TRUE(
        session.begin_start_session(
            UINT32_C(100)).has_value()
    );

    session.mark_pending_transmitted(1000U);

    session.reset();

    EXPECT_FALSE(
        session.retry_pending_transaction().has_value()
    );

    EXPECT_FALSE(
        session.retry_if_due(2000U).has_value()
    );

    EXPECT_TRUE(
        session.begin_start_session(
            UINT32_C(200)).has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    ResetPreservesLifecycleSequenceCounter)
{
    Stm32MotionSession session;

    const auto first =
        session.begin_start_session(
            UINT32_C(100));

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->sequence, 1U);

    session.reset();

    const auto second =
        session.begin_start_session(
            UINT32_C(200));

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->sequence, 2U);
}

TEST(
    Stm32MotionSessionTest,
    RejectsWheelVelocityBeforeMotionSessionIsActive)
{
    Stm32MotionSession session;

    EXPECT_FALSE(
        session.build_wheel_velocity(
            100,
            100).has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    BuildsWheelVelocityForActiveMotionSession)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(1234);

    Stm32MotionSession session;

    const auto start =
        session.begin_start_session(
            motion_session_id);

    ASSERT_TRUE(start.has_value());

    const MotionResponsePayload response_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            start->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &response_payload,
        response.payload);

    ASSERT_TRUE(
        session.handle_response(
            response).has_value()
    );

    const auto wheel =
        session.build_wheel_velocity(
            100,
            -200);

    ASSERT_TRUE(wheel.has_value());

    EXPECT_EQ(
        wheel->message_type,
        PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY
    );

    EXPECT_EQ(
        wheel->sequence,
        1U
    );

    EXPECT_EQ(
        wheel->payload_length,
        WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE
    );

    WheelVelocityPayload decoded = {};

    wheel_velocity_payload_decode(
        wheel->payload,
        &decoded);

    EXPECT_EQ(
        decoded.motion_session_id,
        motion_session_id
    );

    EXPECT_EQ(
        decoded.command.left_velocity_mm_s,
        100
    );

    EXPECT_EQ(
        decoded.command.right_velocity_mm_s,
        -200
    );
}

TEST(
    Stm32MotionSessionTest,
    RejectsWheelVelocityAfterMotionSessionEnds)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(1234);

    Stm32MotionSession session;

    const auto start =
        session.begin_start_session(
            motion_session_id);

    ASSERT_TRUE(start.has_value());

    MotionResponsePayload start_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame start_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            start->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &start_payload,
        start_response.payload);

    ASSERT_TRUE(
        session.handle_response(
            start_response).has_value()
    );

    const auto end =
        session.begin_end_session(
            motion_session_id);

    ASSERT_TRUE(end.has_value());

    MotionResponsePayload end_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_END_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_OK
    };

    ProtocolFrame end_response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            end->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &end_payload,
        end_response.payload);

    ASSERT_TRUE(
        session.handle_response(
            end_response).has_value()
    );

    EXPECT_FALSE(
        session.build_wheel_velocity(
            100,
            100).has_value()
    );
}

TEST(
    Stm32MotionSessionTest,
    FailedStartDoesNotActivateMotionSession)
{
    constexpr std::uint32_t motion_session_id =
        UINT32_C(1234);

    Stm32MotionSession session;

    const auto start =
        session.begin_start_session(
            motion_session_id);

    ASSERT_TRUE(start.has_value());

    const MotionResponsePayload response_payload =
    {
        .command =
            MOTION_LIFECYCLE_COMMAND_START_SESSION,

        .motion_session_id =
            motion_session_id,

        .result =
            MOTION_RESPONSE_REJECTED_UNSAFE
    };

    ProtocolFrame response =
    {
        .message_type =
            PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE,

        .sequence =
            start->sequence,

        .payload_length =
            MOTION_RESPONSE_WIRE_SIZE
    };

    motion_response_encode(
        &response_payload,
        response.payload);

    const auto result =
        session.handle_response(
            response);

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result.value(),
        MOTION_RESPONSE_REJECTED_UNSAFE
    );

    EXPECT_FALSE(
        session.build_wheel_velocity(
            100,
            100).has_value()
    );
}