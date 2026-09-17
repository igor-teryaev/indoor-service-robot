#include <gtest/gtest.h>

#include "motion_ack_codec.h"
#include "motion_ack_payload.h"
#include "motion_command_frames.h"

#include "motion_response_codec.h"
#include "protocol_frame_encoder.h"
#include "protocol_frame_format.h"
#include "protocol_message_type.h"

TEST(MotionCommandFramesTest, BuildsAckFrameFromLifecycleAction)
{
    MotionLifecycleAction action{};

    action.send_ack = true;

    action.ack_transaction = MotionTransaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = 107U
    };

    const MotionCommandFrames frames =
        motion_command_frames_build(&action);

    ASSERT_TRUE(frames.ack_valid);

    EXPECT_EQ(
        frames.ack_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK
    );

    EXPECT_EQ(
        frames.ack_frame.sequence,
        42U
    );

    EXPECT_EQ(
        frames.ack_frame.payload_length,
        MOTION_ACK_WIRE_SIZE
    );

    MotionAckPayload payload{};

    motion_ack_decode(
        frames.ack_frame.payload,
        &payload
    );

    EXPECT_EQ(
        payload.status,
        MOTION_ACK_ACCEPTED
    );

    EXPECT_FALSE(frames.response_valid);
}

TEST(MotionCommandFramesTest, BuildsAckAndResponseForDifferentTransactions)
{
    MotionLifecycleAction action{};

    action.send_ack = true;
    action.ack_transaction = MotionTransaction{
        .sequence = 11U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(108)
    };

    action.send_response = true;
    action.response_transaction = MotionTransaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    action.response_result =
        MOTION_RESPONSE_SUPERSEDED;

    const MotionCommandFrames frames =
        motion_command_frames_build(&action);

    ASSERT_TRUE(frames.ack_valid);
    ASSERT_TRUE(frames.response_valid);

    EXPECT_EQ(
        frames.ack_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK
    );

    EXPECT_EQ(
        frames.ack_frame.sequence,
        11U
    );

    MotionAckPayload ack_payload{};

    motion_ack_decode(
        frames.ack_frame.payload,
        &ack_payload
    );

    EXPECT_EQ(
        ack_payload.status,
        MOTION_ACK_ACCEPTED
    );

    EXPECT_EQ(
        frames.response_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE
    );

    EXPECT_EQ(
        frames.response_frame.sequence,
        10U
    );

    EXPECT_EQ(
        frames.response_frame.payload_length,
        MOTION_RESPONSE_WIRE_SIZE
    );

    MotionResponsePayload response_payload{};

    motion_response_decode(
        frames.response_frame.payload,
        &response_payload
    );

    EXPECT_EQ(
        response_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION
    );

    EXPECT_EQ(
        response_payload.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_EQ(
        response_payload.result,
        MOTION_RESPONSE_SUPERSEDED
    );
}

TEST(MotionCommandFramesTest, BuildsResponseWithoutAck)
{
    MotionLifecycleAction action{};

    action.send_response = true;

    action.response_transaction = MotionTransaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_END_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    action.response_result =
        MOTION_RESPONSE_OK;

    const MotionCommandFrames frames =
        motion_command_frames_build(&action);

    EXPECT_FALSE(frames.ack_valid);

    ASSERT_TRUE(frames.response_valid);

    EXPECT_EQ(
        frames.response_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE
    );

    EXPECT_EQ(
        frames.response_frame.sequence,
        42U
    );

    EXPECT_EQ(
        frames.response_frame.payload_length,
        MOTION_RESPONSE_WIRE_SIZE
    );

    MotionResponsePayload payload{};

    motion_response_decode(
        frames.response_frame.payload,
        &payload
    );

    EXPECT_EQ(
        payload.command,
        MOTION_LIFECYCLE_COMMAND_END_SESSION
    );

    EXPECT_EQ(
        payload.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_EQ(
        payload.result,
        MOTION_RESPONSE_OK
    );
}

TEST(MotionCommandFramesTest, AckFrameEncodesToExpectedWireBytes)
{
    MotionLifecycleAction action{};

    action.send_ack = true;

    action.ack_transaction = MotionTransaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionCommandFrames frames =
        motion_command_frames_build(&action);

    ASSERT_TRUE(frames.ack_valid);

    std::array<uint8_t, PROTOCOL_FRAME_MAX_WIRE_SIZE> wire{};

    const size_t wire_size =
        protocol_frame_encode(
            &frames.ack_frame,
            wire.data()
        );

    const std::array<uint8_t, 11> expected{
        0xA5U,
        0x5AU,
        0x01U,
        0x11U,
        0x00U,
        0x2AU,
        0x00U,
        0x01U,
        0x00U,
        0x7AU,
        0x5FU
    };

    ASSERT_EQ(
        wire_size,
        expected.size()
    );

    EXPECT_TRUE(
        std::equal(
            expected.begin(),
            expected.end(),
            wire.begin()
        )
    );
}

TEST(MotionCommandFramesTest, ResponseFrameEncodesToExpectedWireBytes)
{
    MotionLifecycleAction action{};

    action.send_response = true;

    action.response_transaction = MotionTransaction{
        .sequence = 10U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    action.response_result =
        MOTION_RESPONSE_SUPERSEDED;

    const MotionCommandFrames frames =
        motion_command_frames_build(&action);

    ASSERT_TRUE(frames.response_valid);

    std::array<uint8_t, PROTOCOL_FRAME_MAX_WIRE_SIZE> wire{};

    const size_t wire_size =
        protocol_frame_encode(
            &frames.response_frame,
            wire.data()
        );

    const std::array<uint8_t, 16> expected{
        0xA5U,
        0x5AU,
        0x01U,
        0x12U,
        0x00U,
        0x0AU,
        0x00U,
        0x06U,
        0x01U,
        0x00U,
        0x00U,
        0x00U,
        0x6BU,
        0x04U,
        0xEEU,
        0x4BU
    };

    ASSERT_EQ(
        wire_size,
        expected.size()
    );

    EXPECT_TRUE(
        std::equal(
            expected.begin(),
            expected.end(),
            wire.begin()
        )
    );
}