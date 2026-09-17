#include <gtest/gtest.h>

#include <cstdint>

#include "motion_command_handler.h"
#include "motion_lifecycle_command_type.h"
#include "protocol_message_type.h"

TEST(MotionCommandHandlerTest, StartWirePayloadBeginsMotionSession)
{
    MotionLifecycleCoordinator coordinator{};
    motion_lifecycle_coordinator_init(&coordinator);

    ProtocolFrame frame{};

    frame.message_type =
        PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND;

    frame.sequence = 42U;

    frame.payload_length = 5U;

    frame.payload[0] =
        MOTION_LIFECYCLE_COMMAND_START_SESSION;

    frame.payload[1] = 0x00U;
    frame.payload[2] = 0x00U;
    frame.payload[3] = 0x00U;
    frame.payload[4] = 0x6BU;  // session_id = 107

    const MotionLifecycleAction action =
        motion_command_handler_handle(
            &coordinator,
            &frame
        );

    EXPECT_EQ(
        coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING
    );

    EXPECT_EQ(
        coordinator.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(
        coordinator.reliable_receiver.pending_valid
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.pending_transaction.sequence,
        42U
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.pending_transaction.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION
    );

    EXPECT_EQ(
        coordinator.reliable_receiver.pending_transaction.motion_session_id,
        UINT32_C(107)
    );

    EXPECT_TRUE(action.send_ack);

    EXPECT_EQ(
        action.ack_transaction.sequence,
        42U
    );

    EXPECT_FALSE(action.send_response);

    EXPECT_EQ(
        action.operation,
        MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED
    );

    EXPECT_NE(
        action.operation_id,
        0U
    );

    EXPECT_EQ(
        action.operation_id,
        coordinator.active_operation_id
    );
}