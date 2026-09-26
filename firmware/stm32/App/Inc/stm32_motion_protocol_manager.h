#ifndef STM32_MOTION_PROTOCOL_MANAGER_H
#define STM32_MOTION_PROTOCOL_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "motion_lifecycle_coordinator.h"
#include "protocol_frame.h"
#include "wheel_velocity_feedforward.h"

typedef enum
{
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_INVALID_ARGUMENT = 0,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_INITIALIZED,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_SYNCHRONIZED,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_NOT_MOTION_MESSAGE,

    STM32_MOTION_PROTOCOL_MANAGER_RESULT_NO_ACTIVE_SESSION,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_SESSION_MISMATCH,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_CONVERSION_FAILED,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_APPLY_FAILED,

    STM32_MOTION_PROTOCOL_MANAGER_RESULT_PROCESSED,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_PENDING,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_STOP_FAILED,
    STM32_MOTION_PROTOCOL_MANAGER_RESULT_TRANSMIT_FAILED
} Stm32MotionProtocolManagerResult;

typedef struct
{
    bool initialized;

    MotionLifecycleCoordinator coordinator;

    /*
     * Non-zero while an ENSURE_STOPPED lifecycle operation
     * is waiting for motion_command_guard to complete.
     */
    uint32_t pending_stop_operation_id;
    WheelVelocityFeedforwardConfig feedforward_config;
} Stm32MotionProtocolManager;

#ifdef __cplusplus
extern "C" {
#endif

bool stm32_motion_protocol_manager_init(
    Stm32MotionProtocolManager *manager,
    const WheelVelocityFeedforwardConfig *feedforward_config);

/*
 * Invalidates the current motion session.
 *
 * This is a logical/session reset only.
 * It does not command the motors to stop.
 *
 * The caller is responsible for ensuring the physical stop
 * when synchronization is lost or the transport is reset.
 */
void stm32_motion_protocol_manager_reset(
    Stm32MotionProtocolManager *manager);

/*
 * Handles an inbound motion-related protocol frame.
 *
 * link_synchronized is supplied by the runtime so this module
 * does not depend directly on uart_link_manager.
 *
 * now_ms timestamps accepted WHEEL_VELOCITY commands for the
 * independent motion watchdog.
 */
Stm32MotionProtocolManagerResult
stm32_motion_protocol_manager_handle(
    Stm32MotionProtocolManager *manager,
    const ProtocolFrame *frame,
    bool link_synchronized,
    uint32_t now_ms);

/*
 * Advances pending physical operations and the independent
 * motion-command watchdog.
 */
Stm32MotionProtocolManagerResult
stm32_motion_protocol_manager_update(
    Stm32MotionProtocolManager *manager,
    uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
