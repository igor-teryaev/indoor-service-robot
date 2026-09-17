#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "motion_command_frames.h"
#include "motion_command_handler.h"
#include "motion_lifecycle_coordinator.h"
#include "protocol_frame_decoder.h"
#include "protocol_frame_encoder.h"
#include "protocol_frame_format.h"
#include "protocol_ingress_router.h"

TEST(MotionProtocolFlowTest, StartWireProducesAckWire)
{
    const std::array<uint8_t, 15> input_wire{
        0xA5U,
        0x5AU,
        0x01U,
        0x10U,  // MOTION_COMMAND
        0x00U,
        0x2AU,  // sequence = 42
        0x00U,
        0x05U,  // payload length = 5
        0x01U,  // START
        0x00U,
        0x00U,
        0x00U,
        0x6BU,  // session_id = 107
        0x46U,
        0x59U   // CRC16/CCITT-FALSE
    };

    ProtocolFrameDecoder decoder{};
    protocol_frame_decoder_init(&decoder);

    const ProtocolFrame* decoded_frame = nullptr;

    for (const uint8_t byte : input_wire)
    {
        const bool completed =
            protocol_frame_decoder_feed_byte(
                &decoder,
                byte,
                &decoded_frame
            );

        if (&byte != &input_wire.back())
        {
            // Тут нічого не перевіряємо:
            // range-for дає копію byte, тому адреси не мають сенсу.
        }

        if (completed)
        {
            break;
        }
    }

    ASSERT_NE(decoded_frame, nullptr);

    EXPECT_EQ(
        protocol_ingress_route(decoded_frame),
        PROTOCOL_INGRESS_ROUTE_MOTION_COMMAND
    );

    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionLifecycleAction action =
        motion_command_handler_handle(
            &coordinator,
            decoded_frame
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    ASSERT_TRUE(action.send_ack);

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    EXPECT_NE(
        action.operation_id,
        0U
    );

    const MotionCommandFrames frames =
        motion_command_frames_build(&action);

    ASSERT_TRUE(frames.ack_valid);

    std::array<uint8_t, PROTOCOL_FRAME_MAX_WIRE_SIZE> output_wire{};

    const size_t output_size =
        protocol_frame_encode(
            &frames.ack_frame,
            output_wire.data()
        );

    const std::array<uint8_t, 11> expected_ack_wire{
        0xA5U,
        0x5AU,
        0x01U,
        0x11U,  // MOTION_ACK
        0x00U,
        0x2AU,  // sequence = 42
        0x00U,
        0x01U,  // payload length = 1
        0x00U,  // ACCEPTED
        0x7AU,
        0x5FU   // CRC16/CCITT-FALSE
    };

    ASSERT_EQ(
        output_size,
        expected_ack_wire.size()
    );

    EXPECT_TRUE(
        std::equal(
            expected_ack_wire.begin(),
            expected_ack_wire.end(),
            output_wire.begin()
        )
    );
}

TEST(MotionProtocolFlowTest, SuccessfulStartCompletionProducesResponseWire)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    const MotionTransaction start_transaction{
        .sequence = 42U,
        .command = MOTION_LIFECYCLE_COMMAND_START_SESSION,
        .motion_session_id = UINT32_C(107)
    };

    const MotionLifecycleAction start_action =
        motion_lifecycle_coordinator_handle_transaction(
            &coordinator,
            &start_transaction
        );

    ASSERT_EQ(
        start_action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    ASSERT_NE(
        start_action.operation_id,
        0U
    );

    const MotionLifecycleAction completion_action =
        motion_lifecycle_coordinator_complete_operation(
            &coordinator,
            start_action.operation_id,
            MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
        );

    ASSERT_TRUE(completion_action.send_response);
    EXPECT_FALSE(completion_action.send_ack);

    const MotionCommandFrames frames =
        motion_command_frames_build(
            &completion_action
        );

    EXPECT_FALSE(frames.ack_valid);
    ASSERT_TRUE(frames.response_valid);

    std::array<uint8_t, PROTOCOL_FRAME_MAX_WIRE_SIZE> output_wire{};

    const size_t output_size =
        protocol_frame_encode(
            &frames.response_frame,
            output_wire.data()
        );

    const std::array<uint8_t, 16> expected_response_wire{
        0xA5U,
        0x5AU,
        0x01U,
        0x12U,  // MOTION_RESPONSE
        0x00U,
        0x2AU,  // sequence = 42
        0x00U,
        0x06U,  // payload length = 6
        0x01U,  // START
        0x00U,
        0x00U,
        0x00U,
        0x6BU,  // session_id = 107
        0x00U,  // OK
        0x09U,
        0x33U   // CRC16/CCITT-FALSE
    };

    ASSERT_EQ(
        output_size,
        expected_response_wire.size()
    );

    EXPECT_TRUE(
        std::equal(
            expected_response_wire.begin(),
            expected_response_wire.end(),
            output_wire.begin()
        )
    );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );
}