# System Architecture

The robot is split into a high-level Linux computer and a deterministic low-level
controller. At implementation checkpoint `a99f455`, the C++ control core and shared
C99 protocol/lifecycle library run in host tests. The STM32 firmware and live
transport are still to be integrated.

## Controller responsibilities

Planned Raspberry Pi responsibilities:

- camera acquisition and computer vision;
- object detection and localization;
- high-level navigation and behavior;
- autonomous motion commands;
- Wi-Fi communication with the operator PC.

The C++ control core in `software/rpi` already implements control-authority
arbitration, safety state, coordinated stopping and recovery, a motion watchdog,
kinematics, and a wheel-level motor interface. These components are host tested.

Planned STM32F446RE responsibilities:

- motor control, wheel encoder processing, and velocity control;
- motion state machine, command timeout, and fail-safe stop;
- hardware fault handling and low-level sensors;
- ELRS/CRSF manual control;
- execution and completion reporting for lifecycle `ENSURE_STOPPED` actions.

`firmware/stm32` is currently a placeholder. The protocol lifecycle coordinator
does not directly drive motors or enforce the C++ safety/control-authority state.

## Communication

Planned physical architecture:

```text
RadioMaster Pocket -> ELRS receiver -> CRSF / UART -> STM32F446RE
                                                         |
                                                    UART / CAN
                                                         |
                                                    Raspberry Pi
                                                         |
                                                    Camera / Wi-Fi
```

The shared `robot_protocol` library implements transport-independent framing and
payload codecs. The frame format uses `A5 5A` magic, version 1, message type,
16-bit sequence and payload length, up to 64 payload bytes, and CRC16/CCITT-FALSE.
Multi-byte wire fields use big-endian encoding. The CRC covers version through
the end of the payload. A streaming decoder handles frame validation and
resynchronization; the ingress router checks message type and exact payload size.

| Message | Payload size | Implemented boundary |
| --- | --- | --- |
| `LINK_SYNC`, `LINK_SYNC_OK` | 8 bytes | Codec and ingress route |
| `HEARTBEAT` | 5 bytes | Codec and ingress route |
| `MOTION_COMMAND` | 5 bytes | Codec, ingress route, and lifecycle handler |
| `MOTION_ACK` | 1 byte | Codec, ingress route, and outbound frame builder |
| `MOTION_RESPONSE` | 6 bytes | Codec, ingress route, and outbound frame builder |
| `WHEEL_VELOCITY` | 8 bytes | Session-tagged wheel command codec and ingress route |

Link synchronization and heartbeat codecs do not yet constitute a running link
manager. Wheel-velocity routing does not yet gate or apply commands to STM32 motors.

## Wire-to-lifecycle flow

The host-tested flow is composed from these functions:

```text
received bytes
    -> protocol_frame_decoder_feed_byte()
    -> protocol_ingress_route(): MOTION_COMMAND with exact 5-byte payload
    -> motion_command_handler_handle()
    -> MotionTransaction { sequence, command, motion_session_id }
    -> motion_lifecycle_coordinator_handle_transaction()
    -> MotionLifecycleAction
         | ACK / response fields -> motion_command_frames_build()
         |                       -> protocol_frame_encode() -> outbound bytes
         |
         + ENSURE_STOPPED + operation_id -> future STM32 motor/safety adapter
                                              |
                                   asynchronous SUCCESS / FAILED
                                              |
                        motion_lifecycle_coordinator_complete_operation()
                                              |
                            MotionLifecycleAction -> frames -> outbound bytes
```

The runtime caller must route and validate a decoded frame before calling
`motion_command_handler_handle()`: the handler assumes a valid motion-command
frame and does not repeat the type/length checks. The caller also dispatches the
returned physical action and transmits the generated frames; neither adapter
performs hardware I/O. The decoder owns its returned frame, so the caller must
consume or copy it before feeding more bytes.

`motion_command_frames_build()` uses `ack_transaction` and `response_transaction`
independently. One action can ACK a new command and return `SUPERSEDED` for an
older command, with different sequence numbers. Completion can generate a response
without an ACK.

An ACK carries `ACCEPTED`; it acknowledges receipt/handling, not a successful
lifecycle transition. Even a command ending with `SESSION_MISMATCH`,
`BUSY_STOPPING`, or `INVALID_COMMAND` can receive an ACK and a terminal response.

## Reliable receiver and transaction identity

A `MotionTransaction` is identified by the complete tuple
`(sequence, command, motion_session_id)`. The receiver tracks a sequence frontier,
one pending transaction, and one terminal result; it is not a history of every
completed command.

Sequence ordering is modulo 65536. Relative to the latest sequence, a delta of
1 through 32767 is newer, zero is the same sequence, and 32768 through 65535 is
stale. This supports wraparound within that ordering window.

| Receiver decision | Coordinator behavior |
| --- | --- |
| `NEW` | Apply lifecycle rules and advance the sequence frontier |
| `PENDING_RETRY` | Repeat the ACK without restarting the physical operation |
| `COMPLETED_RETRY` | Replay the ACK and cached terminal response |
| `STALE` | Produce no action and preserve lifecycle state |
| `COLLISION` | Same latest sequence with a different transaction: produce no action |
| `INVALID` | Produce no action |

Retry classification applies to the latest sequence. An older pending operation
may remain active after a newer command receives an immediate terminal response,
but retries of that older sequence are stale. Completing the older pending
operation still emits its response and preserves the newer terminal cache.

This is receiver-side duplicate handling. Sender retransmission timers, live
delivery, and reset/reconnect policy remain runtime integration work.

## Motion lifecycle coordinator

The states are `NO_SESSION`, `STARTING`, `ACTIVE`, and `ENDING`. The following
rules apply to commands classified as `NEW`:

| State and command | Result |
| --- | --- |
| `NO_SESSION` + START | ACK, enter `STARTING`, request `ENSURE_STOPPED` |
| `NO_SESSION` + END | ACK + `ALREADY_ENDED` |
| `ACTIVE` + START for the same session | ACK + `ALREADY_ACTIVE` |
| `ACTIVE` + START for another session | ACK, enter `STARTING` for the new session, request `ENSURE_STOPPED` |
| `ACTIVE` + END for the current session | ACK, enter `ENDING`, request `ENSURE_STOPPED` |
| `STARTING` + START | ACK the new command, supersede the pending START, retain the physical stop |
| `STARTING` + END for the current session | ACK the END, supersede the START, enter `ENDING`, retain the physical stop |
| `ENDING` + START | ACK + `BUSY_STOPPING`; the pending END continues |
| `ENDING` + END for the current session | ACK the new END, supersede the pending END, retain the physical stop |
| Any session-bearing state + END for a different session | ACK + `SESSION_MISMATCH`; preserve the current lifecycle |
| Unknown command | ACK + `INVALID_COMMAND` |

Superseding a pending command returns `SUPERSEDED` for its original transaction.
The ongoing stop completes whichever transaction currently owns it.

Successful stop completion in `STARTING` produces `OK` and enters `ACTIVE`.
Successful completion in `ENDING` produces `OK` and enters `NO_SESSION`.
Failure in either state produces `STOP_FAILED` and clears the session.
`ACTIVE` means the protocol session is established; it does not itself command
wheel motion. Likewise, `NO_SESSION` after failure is not proof that motors stopped.
The future motor/safety adapter must retain appropriate fault and motion inhibition.

## Operation ID semantics

`operation_id` is a local `uint32_t` identity for an asynchronous physical stop.
It is separate from the wire sequence and `motion_session_id`, and is not included
in command, ACK, or response payloads.

- A newly requested `ENSURE_STOPPED` receives a non-zero ID. The generator skips
  zero on wraparound; IDs may repeat after a full non-zero `uint32_t` cycle.
- The adapter captures the ID from that action and returns it with
  `SUCCESS` or `FAILED` to `motion_lifecycle_coordinator_complete_operation()`.
- Zero, wrong, stale, and duplicate completion IDs are ignored. A completion must
  match the active operation while a transaction is pending in `STARTING` or
  `ENDING`; an accepted completion clears the active ID.
- Supersede changes the pending transaction owner, not the ongoing physical
  operation. The new action has operation `NONE` and ID zero; the coordinator
  retains the original active ID. The adapter must keep the ID captured when the
  stop was first requested.
- `reset()` invalidates the session, receiver transaction state, and active
  operation while preserving the ID generation counter. A late completion from
  before reset therefore cannot complete the next operation, subject to the full
  ID-cycle limit above. Reset itself does not request or confirm a physical stop.
- `init()` starts a fresh coordinator lifetime and resets the generation counter.
  Call it only when no asynchronous completions from a previous lifetime can
  arrive. Use `reset()` for runtime protocol/session resets.

These lifetime requirements are also documented in
[`motion_lifecycle_coordinator.h`](../protocol/motion_lifecycle_coordinator.h).

## Safety and integration boundary

Control priority remains E-STOP/hardware fault, then manual ELRS control, then
Raspberry Pi autonomous control. Motion commands must have bounded validity;
loss of the active source must stop motion. Recovery must require a confirmed
safe stop and a fresh valid motion command, without replaying stale movement.
STM32 must enforce low-level safety independently of the Raspberry Pi application.

At this checkpoint, the protocol coordinator has no motor feedback, elapsed-time
input, or connection to safety arbitration. `REJECTED_UNSAFE` is a defined wire
result but is not emitted by this coordinator. Runtime safety gating, operation
deadlines, serialized event ownership, link-loss handling, and real stop
confirmation remain integration work in the [backlog](architecture_backlog.md).

## Validation checkpoint

A fresh host build of `a99f455` passed **192/192 CTest tests** on 2026-09-17,
covering protocol, C++ control-core, and cross-component suites. Protocol tests
include retries/collisions/stale sequences, supersede behavior, late completion
after reset, frame construction, and fixed byte vectors for START -> ACK and
successful START completion -> response.

This validates host-side logic and wire encoding. The physical STM32 stop adapter,
live transport, and hardware timing have not yet been validated. Build commands
and recent commit checkpoints are in the [README](../README.md).
