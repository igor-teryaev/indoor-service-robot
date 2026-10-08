#include "stm32_motion_protocol_manager.h"

#include <stddef.h>

#include "motion_command_guard.h"
#include "motion_command_frames.h"
#include "motion_stop.h"
#include "motion_lifecycle_command_codec.h"
#include "protocol_ingress_router.h"
#include "uart_protocol_transmitter.h"
#include "wheel_velocity_payload_codec.h"

static Stm32MotionProtocolManagerResult process_action(
    Stm32MotionProtocolManager *manager,
    const MotionLifecycleAction *action,
    uint32_t now_ms)
{
    bool transmit_failed = false;
    const MotionCommandFrames frames =
        motion_command_frames_build(action);

    if (frames.ack_valid)
    {
        if (!uart_protocol_transmitter_send(
                &frames.ack_frame))
        {
            transmit_failed = true;
        }
    }

    if (frames.response_valid)
    {
        if (!uart_protocol_transmitter_send(
                &frames.response_frame))
        {
            transmit_failed = true;
        }
    }

    if (action->operation ==
    MOTION_LIFECYCLE_OPERATION_ENSURE_STOPPED)
    {
        manager->pending_stop_operation_id =
            action->operation_id;

        manager->stop_confirmation_started = false;

        if (!motion_command_guard_stop())
        {
            if (transmit_failed)
            {
                return
                    STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED;
            }

            return
                STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING;
        }

        if (transmit_failed)
        {
            return
                STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED;
        }

        return
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING;
    }

    return transmit_failed
        ? STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED
        : STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED;
}

bool stm32_motion_protocol_manager_init(
    Stm32MotionProtocolManager *manager,
    const WheelVelocityFeedforwardConfig *feedforward_config)
{
    if (manager == NULL)
    {
        return false;
    }

    *manager = (Stm32MotionProtocolManager){0};

    if ((feedforward_config == NULL) ||
        (feedforward_config->max_velocity_mm_s == 0U) ||
        (feedforward_config->minimum_start_command >
         MOTOR_DRIVER_COMMAND_MAX))
    {
        return false;
    }

    motion_lifecycle_coordinator_init(
        &manager->coordinator);

    manager->feedforward_config =
        *feedforward_config;

    manager->initialized = true;

    return true;
}

void stm32_motion_protocol_manager_reset(
    Stm32MotionProtocolManager *manager)
{
    if ((manager == NULL) ||
        !manager->initialized)
    {
        return;
    }

    motion_lifecycle_coordinator_reset(
        &manager->coordinator);

    motion_stop_cancel();

    manager->pending_stop_operation_id = 0U;
    manager->stop_confirmation_started = false;
}

Stm32MotionProtocolManagerResult stm32_motion_protocol_manager_handle(
    Stm32MotionProtocolManager *manager,
    const ProtocolFrame *frame,
    bool link_synchronized,
    uint32_t now_ms)
{
    if ((manager == NULL) ||
        (frame == NULL))
    {
        return STM32_MOTION_PROTOCOL_MANAGER_RESULT_INVALID_ARGUMENT;
    }

    if (!manager->initialized)
    {
        return STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_INITIALIZED;
    }

    const ProtocolIngressRoute route =
        protocol_ingress_route(frame);

    if ((route != PROTOCOL_INGRESS_ROUTE_MOTION_COMMAND) &&
        (route != PROTOCOL_INGRESS_ROUTE_WHEEL_VELOCITY))
    {
        return STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_MOTION_MESSAGE;
    }

    if (!link_synchronized)
    {
        return STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_SYNCHRONIZED;
    }

    if (route == PROTOCOL_INGRESS_ROUTE_MOTION_COMMAND)
    {
        MotionLifecycleCommandPayload payload = {0};

        motion_lifecycle_command_decode(
            frame->payload,
            &payload);

        const MotionTransaction transaction =
        {
            .sequence = frame->sequence,
            .command = payload.command,
            .motion_session_id = payload.motion_session_id
        };

        const MotionLifecycleAction action =
            motion_lifecycle_coordinator_handle_transaction(
                &manager->coordinator,
                &transaction);

        return process_action(
            manager,
            &action,
            now_ms);
    }

    /*
     * WHEEL_VELOCITY
     */

    if (manager->coordinator.state != MOTION_LIFECYCLE_STATE_ACTIVE)
    {
        return STM32_MOTION_PROTOCOL_MANAGER_RESULT_NO_ACTIVE_SESSION;
    }

    WheelVelocityPayload payload = {0};

    wheel_velocity_payload_decode(
        frame->payload,
        &payload);

    if (payload.motion_session_id != manager->coordinator.motion_session_id)
    {
        return STM32_MOTION_PROTOCOL_MANAGER_RESULT_SESSION_MISMATCH;
    }

    MotorDriverCommand motor_command = {0};

    if (!wheel_velocity_feedforward_convert(
            &manager->feedforward_config,
            &payload.command,
            &motor_command))
    {
        return STM32_MOTION_PROTOCOL_MANAGER_RESULT_CONVERSION_FAILED;
    }

    if (!motion_command_guard_apply(
            &motor_command,
            now_ms))
    {
        return STM32_MOTION_PROTOCOL_MANAGER_RESULT_APPLY_FAILED;
    }

    return STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED;
}

Stm32MotionProtocolManagerResult stm32_motion_protocol_manager_update(
    Stm32MotionProtocolManager *manager,
    uint32_t now_ms)
{
    if (manager == NULL)
    {
        return
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_INVALID_ARGUMENT;
    }

    if (!manager->initialized)
    {
        return
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_INITIALIZED;
    }

    const MotionCommandGuardUpdate guard_update =
        motion_command_guard_update(now_ms);

    if (guard_update ==
        MOTION_COMMAND_GUARD_UPDATE_STOP_FAILED)
    {
        return
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED;
    }

    /*
     * No lifecycle ENSURE_STOPPED operation is pending.
     * A STOPPED result here may belong to the watchdog or
     * another safety path, so there is nothing to complete.
     */
    if (manager->pending_stop_operation_id == 0U)
    {
        return
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED;
    }

    if (!manager->stop_confirmation_started)
    {
        if (!motion_stop_begin(
                manager->pending_stop_operation_id,
                now_ms))
        {
            const uint32_t operation_id =
                manager->pending_stop_operation_id;

            manager->pending_stop_operation_id = 0U;

            const MotionLifecycleAction completion_action =
                motion_lifecycle_coordinator_complete_operation(
                    &manager->coordinator,
                    operation_id,
                    MOTION_LIFECYCLE_OPERATION_RESULT_FAILED);

            const Stm32MotionProtocolManagerResult completion_result =
                process_action(
                    manager,
                    &completion_action,
                    now_ms);

            if (completion_result ==
                STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED)
            {
                return completion_result;
            }

            return
                STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED;
        }

        manager->stop_confirmation_started = true;
    }

    const MotionStopCompletion stop_completion =
        motion_stop_update(now_ms);

    if (!stop_completion.completed)
    {
        return
            STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING;
    }

    manager->pending_stop_operation_id = 0U;
    manager->stop_confirmation_started = false;

    const MotionLifecycleOperationResult operation_result =
        (stop_completion.result == MOTION_STOP_RESULT_SUCCESS)
            ? MOTION_LIFECYCLE_OPERATION_RESULT_SUCCESS
            : MOTION_LIFECYCLE_OPERATION_RESULT_FAILED;

    const MotionLifecycleAction completion_action =
        motion_lifecycle_coordinator_complete_operation(
            &manager->coordinator,
            stop_completion.operation_id,
            operation_result);

    const Stm32MotionProtocolManagerResult completion_result =
        process_action(
            manager,
            &completion_action,
            now_ms);

    if (completion_result ==
        STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED)
    {
        return completion_result;
    }

    return (stop_completion.result == MOTION_STOP_RESULT_SUCCESS)
        ? completion_result
        : STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED;
}
