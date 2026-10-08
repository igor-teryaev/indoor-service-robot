#include <gtest/gtest.h>

#include <array>
#include <cstdint>

extern "C"
{
#include "motion_ack_codec.h"
#include "motion_command_guard.h"
#include "motion_lifecycle_command_codec.h"
#include "motion_response_codec.h"
#include "motor_driver_dri0041_port.h"
#include "protocol_message_type.h"
#include "stm32_motion_protocol_manager.h"
#include "uart_protocol_transmitter.h"
#include "wheel_encoder.h"
#include "wheel_encoder_port.h"
}

namespace
{
    WheelEncoderCounts encoder_counts{0U, 0U};

    bool motor_port_init_result = true;
    bool motor_port_apply_result = true;
    uint32_t motor_port_apply_count = 0U;
    Dri0041PortControl last_motor_control{};
    uint32_t motor_port_now_ms = 0U;

    bool encoder_port_init_result = true;

    bool transmit_result = true;
    uint32_t transmit_call_count = 0U;
    std::array<ProtocolFrame, 4> transmitted_frames{};

    ProtocolFrame make_motion_command_frame(
        uint16_t sequence,
        MotionLifecycleCommandType command,
        uint32_t session_id)
    {
        const MotionLifecycleCommandPayload payload =
        {
            .command = command,
            .motion_session_id = session_id
        };

        ProtocolFrame frame =
        {
            .message_type =
                PROTOCOL_MESSAGE_TYPE_MOTION_COMMAND,
            .sequence = sequence,
            .payload_length =
                MOTION_LIFECYCLE_COMMAND_WIRE_SIZE
        };

        motion_lifecycle_command_encode(
            &payload,
            frame.payload);

        return frame;
    }
}

extern "C" bool motor_driver_dri0041_port_init(void)
{
    return motor_port_init_result;
}

extern "C" bool motor_driver_dri0041_port_apply(
    const Dri0041PortControl *control)
{
    ++motor_port_apply_count;

    if (control != nullptr)
    {
        last_motor_control = *control;
    }

    return motor_port_apply_result;
}

extern "C" uint32_t motor_driver_dri0041_port_now_ms(void)
{
    return motor_port_now_ms;
}

extern "C" bool wheel_encoder_port_init(void)
{
    return encoder_port_init_result;
}

extern "C" WheelEncoderCounts wheel_encoder_port_read(void)
{
    return encoder_counts;
}

extern "C" bool uart_protocol_transmitter_send(
    const ProtocolFrame *frame)
{
    if ((frame != nullptr) &&
        (transmit_call_count < transmitted_frames.size()))
    {
        transmitted_frames[transmit_call_count] = *frame;
    }

    ++transmit_call_count;

    return transmit_result;
}

class Stm32MotionStopIntegrationTest
    : public ::testing::Test
{
protected:
    Stm32MotionProtocolManager manager{};

    void SetUp() override
    {
        encoder_counts = {0U, 0U};

        motor_port_init_result = true;
        motor_port_apply_result = true;
        motor_port_apply_count = 0U;
        last_motor_control = {};
        motor_port_now_ms = 0U;

        encoder_port_init_result = true;

        transmit_result = true;
        transmit_call_count = 0U;
        transmitted_frames = {};

        ASSERT_TRUE(wheel_encoder_init());

        ASSERT_TRUE(
            motion_command_guard_init(250U));

        static constexpr WheelVelocityFeedforwardConfig
            FEEDFORWARD_CONFIG =
            {
                .max_velocity_mm_s = 400U,
                .minimum_start_command = 50U
            };

        ASSERT_TRUE(
            stm32_motion_protocol_manager_init(
                &manager,
                &FEEDFORWARD_CONFIG));
    }

    void StartActiveSession(
        uint16_t sequence,
        uint32_t session_id,
        uint32_t handle_ms,
        uint32_t confirmation_start_ms)
    {
        const ProtocolFrame start_frame =
            make_motion_command_frame(
                sequence,
                MOTION_LIFECYCLE_COMMAND_START_SESSION,
                session_id);

        ASSERT_EQ(
            stm32_motion_protocol_manager_handle(
                &manager,
                &start_frame,
                true,
                handle_ms),
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

        ASSERT_EQ(
            stm32_motion_protocol_manager_update(
                &manager,
                confirmation_start_ms),
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

        ASSERT_EQ(
            stm32_motion_protocol_manager_update(
                &manager,
                confirmation_start_ms + 200U),
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

        ASSERT_EQ(
            manager.coordinator.state,
            MOTION_LIFECYCLE_STATE_ACTIVE);

        ASSERT_EQ(
            manager.coordinator.motion_session_id,
            session_id);
    }
};

TEST_F(
    Stm32MotionStopIntegrationTest,
    StartFailsWhenEncoderKeepsMovingUntilPhysicalStopTimeout)
{
    const ProtocolFrame start_frame =
        make_motion_command_frame(
            1U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            123U);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    /*
     * START ACK only. Physical-stop confirmation has not
     * completed yet.
     */
    ASSERT_EQ(transmit_call_count, 1U);

    ASSERT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            200U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    /*
     * Keep producing encoder movement before every update.
     */
    encoder_counts.left = 1U;
    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            500U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    encoder_counts.left = 2U;
    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            900U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    encoder_counts.left = 3U;

    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            1200U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED);

    ASSERT_EQ(transmit_call_count, 2U);

    const ProtocolFrame &response_frame =
        transmitted_frames[1];

    EXPECT_EQ(
        response_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE);

    MotionResponsePayload response_payload{};

    motion_response_decode(
        response_frame.payload,
        &response_payload);

    EXPECT_EQ(
        response_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    EXPECT_EQ(
        response_payload.motion_session_id,
        123U);

    EXPECT_EQ(
        response_payload.result,
        MOTION_RESPONSE_STOP_FAILED);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        0U);
}

TEST_F(
    Stm32MotionStopIntegrationTest,
    EndFailsWhenEncoderKeepsMovingUntilPhysicalStopTimeout)
{
    constexpr uint32_t session_id = 456U;

    StartActiveSession(
        10U,
        session_id,
        100U,
        200U);

    /*
     * Ignore START ACK + terminal response.
     */
    transmit_call_count = 0U;
    transmitted_frames = {};

    const ProtocolFrame end_frame =
        make_motion_command_frame(
            11U,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &end_frame,
            true,
            500U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    /*
     * END ACK is immediate, terminal response is not.
     */
    ASSERT_EQ(transmit_call_count, 1U);

    /*
     * Physical-stop timing begins here, at 600 ms.
     * Its hard deadline is therefore 1600 ms.
     */
    ASSERT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            600U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    encoder_counts.right = 1U;
    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            900U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    encoder_counts.right = 2U;
    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            1300U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    encoder_counts.right = 3U;

    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            1600U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED);

    ASSERT_EQ(transmit_call_count, 2U);

    const ProtocolFrame &response_frame =
        transmitted_frames[1];

    EXPECT_EQ(
        response_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE);

    MotionResponsePayload response_payload{};

    motion_response_decode(
        response_frame.payload,
        &response_payload);

    EXPECT_EQ(
        response_payload.command,
        MOTION_LIFECYCLE_COMMAND_END_SESSION);

    EXPECT_EQ(
        response_payload.motion_session_id,
        session_id);

    EXPECT_EQ(
        response_payload.result,
        MOTION_RESPONSE_STOP_FAILED);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        0U);
}

TEST_F(
    Stm32MotionStopIntegrationTest,
    PendingStartRetryDoesNotRestartPhysicalStopDeadline)
{
    constexpr uint32_t session_id = 789U;

    const ProtocolFrame start_frame =
        make_motion_command_frame(
            20U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    const uint32_t operation_id =
        manager.pending_stop_operation_id;

    ASSERT_NE(operation_id, 0U);

    /*
     * Physical-stop timing starts here.
     * Hard deadline is therefore 1200 ms.
     */
    ASSERT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            200U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    encoder_counts.left = 1U;

    ASSERT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            900U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    /*
     * Pi retries the exact same lifecycle transaction while
     * physical-stop confirmation is still pending.
     *
     * This must repeat the ACK but must not create a new
     * ENSURE_STOPPED operation or restart its deadline.
     */
    EXPECT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            950U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        operation_id);

    encoder_counts.left = 2U;

    /*
     * 1200 ms is still the original hard deadline,
     * measured from confirmation start at 200 ms.
     */
    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            1200U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        0U);

    /*
     * Initial ACK + retry ACK + terminal failure response.
     */
    ASSERT_EQ(transmit_call_count, 3U);

    EXPECT_EQ(
        transmitted_frames[0].message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK);

    EXPECT_EQ(
        transmitted_frames[1].message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK);

    EXPECT_EQ(
        transmitted_frames[2].message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE);

    MotionResponsePayload response_payload{};

    motion_response_decode(
        transmitted_frames[2].payload,
        &response_payload);

    EXPECT_EQ(
        response_payload.result,
        MOTION_RESPONSE_STOP_FAILED);
}

TEST_F(
    Stm32MotionStopIntegrationTest,
    ResetCancelsPendingPhysicalStopConfirmation)
{
    const ProtocolFrame start_frame =
        make_motion_command_frame(
            30U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            987U);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    ASSERT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            200U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    ASSERT_NE(
        manager.pending_stop_operation_id,
        0U);

    ASSERT_EQ(transmit_call_count, 1U);

    /*
     * Simulate link/session reset while physical-stop
     * confirmation is still settling.
     */
    stm32_motion_protocol_manager_reset(
        &manager);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        0U);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        0U);

    EXPECT_FALSE(
        manager.stop_confirmation_started);

    /*
     * Later updates must not resurrect or complete the old
     * physical-stop operation.
     */
    encoder_counts.left = 1U;

    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            2000U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    /*
     * Still only the original START ACK. No stale terminal
     * response may appear after reset.
     */
    EXPECT_EQ(transmit_call_count, 1U);
}