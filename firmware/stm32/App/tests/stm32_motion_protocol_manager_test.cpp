#include <gtest/gtest.h>

#include <array>
#include <cstdint>

extern "C"
{
#include "motion_ack_codec.h"
#include "motion_command_guard.h"
#include "motion_lifecycle_command_codec.h"
#include "motion_response_codec.h"
#include "protocol_message_type.h"
#include "stm32_motion_protocol_manager.h"
#include "uart_protocol_transmitter.h"
#include "wheel_velocity_payload_codec.h"
}

namespace
{
    bool stop_result = true;
    uint32_t stop_call_count = 0U;

    bool apply_result = true;
    uint32_t apply_call_count = 0U;
    MotorDriverCommand applied_command = {};
    uint32_t applied_now_ms = 0U;

    MotionCommandGuardUpdate guard_update_result =
        MOTION_COMMAND_GUARD_UPDATE_NONE;

    bool transmit_result = true;
    uint32_t transmit_call_count = 0U;

    std::array<ProtocolFrame, 4> transmitted_frames = {};

    ProtocolFrame make_motion_command_frame(
        uint16_t sequence,
        MotionLifecycleCommandType command,
        uint32_t motion_session_id)
    {
        const MotionLifecycleCommandPayload payload =
        {
            .command = command,
            .motion_session_id = motion_session_id
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

    ProtocolFrame make_wheel_velocity_frame(
    uint16_t sequence,
    uint32_t motion_session_id,
    int16_t left_velocity_mm_s,
    int16_t right_velocity_mm_s)
    {
        const WheelVelocityPayload payload =
        {
            .motion_session_id = motion_session_id,
            .command =
            {
                .left_velocity_mm_s = left_velocity_mm_s,
                .right_velocity_mm_s = right_velocity_mm_s
            }
        };

        ProtocolFrame frame =
        {
            .message_type =
                PROTOCOL_MESSAGE_TYPE_WHEEL_VELOCITY,
            .sequence = sequence,
            .payload_length =
                WHEEL_VELOCITY_PAYLOAD_WIRE_SIZE
        };

        wheel_velocity_payload_encode(
            &payload,
            frame.payload);

        return frame;
    }
}

extern "C" bool motion_command_guard_stop(void)
{
    ++stop_call_count;
    return stop_result;
}

extern "C" MotionCommandGuardUpdate
motion_command_guard_update(uint32_t now_ms)
{
    (void)now_ms;
    return guard_update_result;
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

extern "C" bool motion_command_guard_apply(
    const MotorDriverCommand *command,
    uint32_t now_ms)
{
    ++apply_call_count;
    applied_now_ms = now_ms;

    if (command != nullptr)
    {
        applied_command = *command;
    }

    return apply_result;
}

class Stm32MotionProtocolManagerTest
    : public ::testing::Test
{
protected:
    Stm32MotionProtocolManager manager = {};

    void SetUp() override
    {
        stop_result = true;
        stop_call_count = 0U;

        apply_result = true;
        apply_call_count = 0U;
        applied_command = {};
        applied_now_ms = 0U;

        guard_update_result =
            MOTION_COMMAND_GUARD_UPDATE_NONE;

        transmit_result = true;
        transmit_call_count = 0U;
        transmitted_frames = {};

        static constexpr WheelVelocityFeedforwardConfig FEEDFORWARD_CONFIG =
        {
            .max_velocity_mm_s = 400U,
            .minimum_start_command = 800U
        };
        ASSERT_TRUE(
            stm32_motion_protocol_manager_init(
                &manager,
                &FEEDFORWARD_CONFIG));
    }
};

TEST_F(
    Stm32MotionProtocolManagerTest,
    StartsSessionAfterSuccessfulStop)
{
    constexpr uint16_t sequence = 42U;
    constexpr uint32_t session_id = 123U;

    const ProtocolFrame frame =
        make_motion_command_frame(
            sequence,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &frame,
            true,
            100U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    EXPECT_EQ(stop_call_count, 1U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        session_id);

    ASSERT_EQ(transmit_call_count, 2U);

    /*
     * First frame: ACK.
     */
    const ProtocolFrame &ack_frame =
        transmitted_frames[0];

    EXPECT_EQ(
        ack_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK);

    EXPECT_EQ(
        ack_frame.sequence,
        sequence);

    EXPECT_EQ(
        ack_frame.payload_length,
        MOTION_ACK_WIRE_SIZE);

    MotionAckPayload ack_payload = {0};

    motion_ack_decode(
        ack_frame.payload,
        &ack_payload);

    EXPECT_EQ(
        ack_payload.status,
        MOTION_ACK_ACCEPTED);

    /*
     * Second frame: terminal response.
     */
    const ProtocolFrame &response_frame =
        transmitted_frames[1];

    EXPECT_EQ(
        response_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE);

    EXPECT_EQ(
        response_frame.sequence,
        sequence);

    EXPECT_EQ(
        response_frame.payload_length,
        MOTION_RESPONSE_WIRE_SIZE);

    MotionResponsePayload response_payload = {0};

    motion_response_decode(
        response_frame.payload,
        &response_payload);

    EXPECT_EQ(
        response_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    EXPECT_EQ(
        response_payload.motion_session_id,
        session_id);

    EXPECT_EQ(
        response_payload.result,
        MOTION_RESPONSE_OK);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    KeepsSessionStartingUntilPendingStopCompletes)
{
    constexpr uint16_t sequence = 43U;
    constexpr uint32_t session_id = 456U;

    const ProtocolFrame frame =
        make_motion_command_frame(
            sequence,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    /*
     * First stop attempt fails.
     * The guard will continue retrying internally.
     */
    stop_result = false;

    const Stm32MotionProtocolManagerResult handle_result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &frame,
            true,
            100U);

    EXPECT_EQ(
        handle_result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    EXPECT_EQ(stop_call_count, 1U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        session_id);

    EXPECT_NE(
        manager.pending_stop_operation_id,
        0U);

    /*
     * Only ACK should have been transmitted so far.
     * There must be no terminal response until the
     * physical stop operation completes.
     */
    ASSERT_EQ(transmit_call_count, 1U);

    EXPECT_EQ(
        transmitted_frames[0].message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK);

    EXPECT_EQ(
        transmitted_frames[0].sequence,
        sequence);

    /*
     * Later, motion_command_guard reports that the
     * pending stop has completed successfully.
     */
    guard_update_result =
        MOTION_COMMAND_GUARD_UPDATE_STOPPED;

    const Stm32MotionProtocolManagerResult update_result =
        stm32_motion_protocol_manager_update(
            &manager,
            101U);

    EXPECT_EQ(
        update_result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        0U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        session_id);

    /*
     * Now the terminal response should appear.
     */
    ASSERT_EQ(transmit_call_count, 2U);

    const ProtocolFrame &response_frame =
        transmitted_frames[1];

    EXPECT_EQ(
        response_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE);

    EXPECT_EQ(
        response_frame.sequence,
        sequence);

    MotionResponsePayload response_payload = {0};

    motion_response_decode(
        response_frame.payload,
        &response_payload);

    EXPECT_EQ(
        response_payload.command,
        MOTION_LIFECYCLE_COMMAND_START_SESSION);

    EXPECT_EQ(
        response_payload.motion_session_id,
        session_id);

    EXPECT_EQ(
        response_payload.result,
        MOTION_RESPONSE_OK);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    ResetClearsActiveSession)
{
    constexpr uint16_t sequence = 44U;
    constexpr uint32_t session_id = 789U;

    const ProtocolFrame frame =
        make_motion_command_frame(
            sequence,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    ASSERT_EQ(
        manager.coordinator.motion_session_id,
        session_id);

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
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    RejectsMotionCommandBeforeSynchronization)
{
    constexpr uint16_t sequence = 45U;
    constexpr uint32_t session_id = 321U;

    const ProtocolFrame frame =
        make_motion_command_frame(
            sequence,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &frame,
            false,
            100U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_SYNCHRONIZED);

    EXPECT_EQ(stop_call_count, 0U);
    EXPECT_EQ(transmit_call_count, 0U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        0U);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        0U);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    RejectsEndForMismatchedSession)
{
    constexpr uint16_t start_sequence = 50U;
    constexpr uint16_t end_sequence = 51U;

    constexpr uint32_t active_session_id = 1000U;
    constexpr uint32_t wrong_session_id = 2000U;

    const ProtocolFrame start_frame =
        make_motion_command_frame(
            start_sequence,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            active_session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    ASSERT_EQ(
        manager.coordinator.motion_session_id,
        active_session_id);

    /*
     * Ignore the START ACK + RESPONSE.
     */
    transmit_call_count = 0U;
    transmitted_frames = {};
    stop_call_count = 0U;

    const ProtocolFrame end_frame =
        make_motion_command_frame(
            end_sequence,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            wrong_session_id);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &end_frame,
            true,
            200U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    /*
     * Wrong-session END must not stop the active session.
     */
    EXPECT_EQ(stop_call_count, 0U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        active_session_id);

    /*
     * The protocol still acknowledges the transaction and
     * returns an explicit terminal SESSION_MISMATCH response.
     */
    ASSERT_EQ(transmit_call_count, 2U);

    const ProtocolFrame &ack_frame =
        transmitted_frames[0];

    EXPECT_EQ(
        ack_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK);

    EXPECT_EQ(
        ack_frame.sequence,
        end_sequence);

    const ProtocolFrame &response_frame =
        transmitted_frames[1];

    EXPECT_EQ(
        response_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE);

    EXPECT_EQ(
        response_frame.sequence,
        end_sequence);

    MotionResponsePayload response_payload = {0};

    motion_response_decode(
        response_frame.payload,
        &response_payload);

    EXPECT_EQ(
        response_payload.command,
        MOTION_LIFECYCLE_COMMAND_END_SESSION);

    EXPECT_EQ(
        response_payload.motion_session_id,
        wrong_session_id);

    EXPECT_EQ(
        response_payload.result,
        MOTION_RESPONSE_SESSION_MISMATCH);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    EndsActiveSessionAfterSuccessfulStop)
{
    constexpr uint16_t start_sequence = 60U;
    constexpr uint16_t end_sequence = 61U;
    constexpr uint32_t session_id = 3000U;

    const ProtocolFrame start_frame =
        make_motion_command_frame(
            start_sequence,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    /*
     * Ignore START traffic.
     */
    transmit_call_count = 0U;
    transmitted_frames = {};
    stop_call_count = 0U;

    const ProtocolFrame end_frame =
        make_motion_command_frame(
            end_sequence,
            MOTION_LIFECYCLE_COMMAND_END_SESSION,
            session_id);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &end_frame,
            true,
            200U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    EXPECT_EQ(stop_call_count, 1U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        0U);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        0U);

    ASSERT_EQ(transmit_call_count, 2U);

    /*
     * First: ACK.
     */
    EXPECT_EQ(
        transmitted_frames[0].message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_ACK);

    EXPECT_EQ(
        transmitted_frames[0].sequence,
        end_sequence);

    /*
     * Second: terminal response.
     */
    const ProtocolFrame &response_frame =
        transmitted_frames[1];

    EXPECT_EQ(
        response_frame.message_type,
        PROTOCOL_MESSAGE_TYPE_MOTION_RESPONSE);

    EXPECT_EQ(
        response_frame.sequence,
        end_sequence);

    MotionResponsePayload response_payload = {0};

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
        MOTION_RESPONSE_OK);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    AppliesWheelVelocityForActiveMatchingSession)
{
    constexpr uint32_t session_id = 123U;

    const ProtocolFrame start_frame =
        make_motion_command_frame(
            70U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    /*
     * Ignore lifecycle traffic from START_SESSION.
     */
    apply_call_count = 0U;
    applied_command = {};
    applied_now_ms = 0U;

    const ProtocolFrame wheel_frame =
        make_wheel_velocity_frame(
            71U,
            session_id,
            100,
            -200);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &wheel_frame,
            true,
            500U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(apply_call_count, 1U);

    /*
     * Test configuration:
     *
     * max_velocity_mm_s = 400
     * minimum_start_command = 800
     *
     *  +100 mm/s -> +850
     *  -200 mm/s -> -900
     */
    EXPECT_EQ(applied_command.left, 850);
    EXPECT_EQ(applied_command.right, -900);

    EXPECT_EQ(applied_now_ms, 500U);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    RejectsWheelVelocityForMismatchedSession)
{
    constexpr uint32_t active_session_id = 123U;
    constexpr uint32_t wrong_session_id = 999U;

    const ProtocolFrame start_frame =
        make_motion_command_frame(
            80U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            active_session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    apply_call_count = 0U;
    applied_command = {};
    applied_now_ms = 0U;

    const ProtocolFrame wheel_frame =
        make_wheel_velocity_frame(
            81U,
            wrong_session_id,
            100,
            100);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &wheel_frame,
            true,
            500U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_SESSION_MISMATCH);

    EXPECT_EQ(apply_call_count, 0U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        active_session_id);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    RejectsWheelVelocityWithoutActiveSession)
{
    constexpr uint32_t session_id = 123U;

    const ProtocolFrame wheel_frame =
        make_wheel_velocity_frame(
            90U,
            session_id,
            100,
            100);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &wheel_frame,
            true,
            500U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_NO_ACTIVE_SESSION);

    EXPECT_EQ(apply_call_count, 0U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        0U);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    RejectsWheelVelocityBeforeSynchronization)
{
    constexpr uint32_t session_id = 123U;

    const ProtocolFrame wheel_frame =
        make_wheel_velocity_frame(
            100U,
            session_id,
            100,
            -100);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &wheel_frame,
            false,
            500U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_SYNCHRONIZED);

    EXPECT_EQ(apply_call_count, 0U);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        0U);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    ReportsApplyFailureForValidWheelVelocity)
{
    constexpr uint32_t session_id = 123U;

    const ProtocolFrame start_frame =
        make_motion_command_frame(
            110U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    apply_call_count = 0U;
    applied_command = {};
    applied_now_ms = 0U;
    apply_result = false;

    const ProtocolFrame wheel_frame =
        make_wheel_velocity_frame(
            111U,
            session_id,
            100,
            200);

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_handle(
            &manager,
            &wheel_frame,
            true,
            500U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_APPLY_FAILED);

    ASSERT_EQ(apply_call_count, 1U);

    EXPECT_EQ(applied_command.left, 850);
    EXPECT_EQ(applied_command.right, 900);
    EXPECT_EQ(applied_now_ms, 500U);

    /*
     * The lifecycle session itself remains active.
     *
     * motion_command_guard_apply() owns the motor-safety
     * response to the failed apply attempt.
     */
    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        session_id);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    WatchdogStopDoesNotEndActiveSession)
{
    constexpr uint32_t session_id = 123U;

    const ProtocolFrame start_frame =
        make_motion_command_frame(
            120U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            session_id);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    /*
     * No lifecycle ENSURE_STOPPED operation is pending.
     */
    ASSERT_EQ(
        manager.pending_stop_operation_id,
        0U);

    /*
     * Simulate the motion watchdog expiring and the guard
     * successfully stopping the motors.
     */
    guard_update_result =
        MOTION_COMMAND_GUARD_UPDATE_STOPPED;

    const Stm32MotionProtocolManagerResult result =
        stm32_motion_protocol_manager_update(
            &manager,
            500U);

    EXPECT_EQ(
        result,
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    /*
     * Physical motion stopped, but the logical session
     * remains active.
     */
    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    EXPECT_EQ(
        manager.coordinator.motion_session_id,
        session_id);

    EXPECT_EQ(
        manager.pending_stop_operation_id,
        0U);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    FailedReinitializationClearsActiveSession)
{
    const ProtocolFrame start_frame =
        make_motion_command_frame(
            130U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            123U);

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED);

    ASSERT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_ACTIVE);

    constexpr WheelVelocityFeedforwardConfig invalid_config =
    {
        .max_velocity_mm_s = 0U,
        .minimum_start_command = 800U
    };

    EXPECT_FALSE(
        stm32_motion_protocol_manager_init(
            &manager,
            &invalid_config));

    EXPECT_FALSE(manager.initialized);
    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_NO_SESSION);
    EXPECT_EQ(manager.coordinator.motion_session_id, 0U);
    EXPECT_EQ(manager.pending_stop_operation_id, 0U);

    EXPECT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            200U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_INITIALIZED);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    ReportsLifecycleTransmitFailure)
{
    const ProtocolFrame start_frame =
        make_motion_command_frame(
            131U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            456U);

    transmit_result = false;

    EXPECT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED);

    EXPECT_EQ(stop_call_count, 1U);
    EXPECT_EQ(transmit_call_count, 2U);
}

TEST_F(
    Stm32MotionProtocolManagerTest,
    ReportsPendingPhysicalStopFailure)
{
    const ProtocolFrame start_frame =
        make_motion_command_frame(
            132U,
            MOTION_LIFECYCLE_COMMAND_START_SESSION,
            789U);

    stop_result = false;

    ASSERT_EQ(
        stm32_motion_protocol_manager_handle(
            &manager,
            &start_frame,
            true,
            100U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING);

    ASSERT_NE(manager.pending_stop_operation_id, 0U);

    guard_update_result =
        MOTION_COMMAND_GUARD_UPDATE_STOP_FAILED;

    EXPECT_EQ(
        stm32_motion_protocol_manager_update(
            &manager,
            101U),
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED);

    EXPECT_EQ(
        manager.coordinator.state,
        MOTION_LIFECYCLE_STATE_STARTING);
    EXPECT_NE(manager.pending_stop_operation_id, 0U);
}
