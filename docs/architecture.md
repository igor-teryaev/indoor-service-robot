# System Architecture

The robot is split into a high-level Linux computer and a deterministic low-level
STM32 controller.

At the current checkpoint, the Raspberry Pi ↔ STM32 motion-control path is running
on real hardware. The Raspberry Pi owns high-level motion demand and communication
runtime state, while the STM32 owns low-level motor actuation, motion-session gating,
and independent fail-safe motion timeout behavior.

## Controller responsibilities

Raspberry Pi responsibilities:

- camera acquisition and computer vision;
- object detection and localization;
- high-level navigation and behavior;
- autonomous motion commands;
- future manual/autonomous control arbitration;
- Wi-Fi communication with the operator PC;
- STM32 link synchronization, heartbeat supervision, disconnect, and reconnect;
- motion lifecycle initiation and retry handling;
- latest-value-wins wheel-demand ownership;
- application-level wheel-command freshness enforcement.

The C++ control core in `software/rpi` also implements control-authority arbitration,
safety state, coordinated stopping and recovery, motion watchdog logic, kinematics,
and wheel-level motion interfaces.

The concrete `Stm32ClientRunner` owns the live Raspberry Pi-side STM32 runtime.
It advances cooperatively through `poll()` without a dedicated thread. It owns the
serial transport, link session, motion session, frame decoder, reconnect state,
current motion-session ID, and latest application wheel demand.

STM32F446RE responsibilities:

- TIM8 PWM motor control through DRV8833;
- motion lifecycle coordination;
- session-aware `WHEEL_VELOCITY` gating;
- independent 250 ms motion-command watchdog;
- immediate motor stop on motion timeout, link loss/reset paths, and lifecycle stop;
- low-level hardware and sensor integration;
- future encoder processing and closed-loop wheel-speed control;
- future ELRS/CRSF manual-control integration.

The current ARC101 platform has no encoder sensors installed. Therefore the active
wheel-velocity path is open-loop feed-forward: a requested wheel velocity is mapped
to a motor-driver command rather than regulated from measured wheel speed.

The intended final drive platform is the Rowenta RR6825WH, where optical feedback
is expected to support later closed-loop velocity control.

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

The Raspberry Pi runtime uses `Stm32LinkSession` and `Stm32ClientRunner` to perform
live link synchronization, heartbeat supervision, disconnect detection, and
reconnect handling over the Linux serial transport.

Motion is intentionally separate from link liveness. Heartbeats maintain the
synchronized link but do not refresh the STM32 motion watchdog.

A motion session is started only when the Raspberry Pi has a fresh non-zero wheel
demand. Session-tagged `WHEEL_VELOCITY` frames are accepted by the STM32 only while
the matching motion session is active. Wheel commands refresh the independent
250 ms STM32 motion watchdog.

On the Raspberry Pi, application wheel demand is latest-value-wins and valid for
200 ms. While demand remains fresh and non-zero, the runner transmits wheel commands
at a 50 ms period. Explicit zero demand or stale demand initiates `MOTION_END`.

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
         + ENSURE_STOPPED + operation_id -> STM32 motion/motor safety adapter
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

This is receiver-side duplicate handling. Raspberry Pi sender retransmission,
live serial delivery, lifecycle retry timing, and reset/reconnect policy are now
implemented by the live runtime around this receiver contract.

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
The STM32 motor/safety layer must retain appropriate fault and motion inhibition independently of protocol-session cleanup.

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

Control priority remains E-STOP/hardware fault, then manual control, then autonomous
control. The current live motion path implements the communication and freshness
part of that safety model; full ELRS/manual-control integration is still future work.

Motion requires two independent conditions:

1. the Raspberry Pi ↔ STM32 link must be synchronized;
2. a matching motion session must be active and continue receiving fresh
   `WHEEL_VELOCITY` commands.

Heartbeats keep the communication link synchronized but do not sustain motor motion.
Only accepted wheel commands refresh the STM32's 250 ms motion watchdog.

The Raspberry Pi additionally treats application wheel demand as valid for 200 ms.
If that demand becomes stale, or if the application explicitly requests zero wheel
velocity, `Stm32ClientRunner` initiates `MOTION_END`.

A communication reset or reconnect invalidates local motion-session state and cached
wheel demand. Restoring the link therefore does not automatically resume previous
movement; the application must provide new post-reconnect motion demand.

Frame transmission on the Raspberry Pi uses a bounded absolute deadline rather than
waiting indefinitely for a writable serial transport.

These mechanisms are complementary rather than interchangeable:

- Raspberry Pi demand freshness limits how long stale application intent can persist;
- motion lifecycle state prevents wheel traffic outside an active matching session;
- the STM32 motion watchdog independently stops motion if wheel traffic disappears;
- link heartbeat supervision detects communication failure and drives reconnect;
- reconnect clears previous demand so old movement cannot be replayed.

Physical stop confirmation using wheel feedback is not yet available on the ARC101
bring-up platform because encoder sensors are not installed. Current ARC101 stop
behavior is therefore validated through commanded motor shutdown and observed
hardware behavior, not measured zero wheel velocity.

## Validation checkpoint

At the current Raspberry Pi motion-runtime checkpoint:

- the full WSL CTest suite passes **342/342 tests**;
- `Stm32MotionSession` has explicit local lifecycle states and rejects wheel traffic
  outside an active matching session;
- Linux serial frame transmission uses a 100 ms absolute deadline;
- `Stm32ClientRunner` owns live synchronization, heartbeat supervision, lifecycle
  retries, reconnect state, and wheel-demand freshness;
- Raspberry Pi PTY tests cover synchronization without motion, demand-driven START,
  wheel transmission after successful START, explicit-zero END, stale-command END,
  latest-value-wins behavior, reconnect without demand replay, and active-motion
  reset/reconnect behavior.

Hardware validation on Raspberry Pi 5 + NUCLEO-F446RE + DRV8833 + ARC101 confirmed:

- synchronization alone does not move the motors;
- fresh non-zero wheel demand starts a motion session and physically starts motion;
- one-shot demand expires and causes motion to stop;
- periodically refreshed demand sustains motion;
- explicit zero demand ends the session and stops the motors;
- Nucleo RESET during active motion immediately stops the motors;
- the Raspberry Pi detects the unsynchronized STM32, reconnects, and does not replay
  the previous wheel demand after resynchronization;
- heartbeat traffic continues independently from motion lifecycle traffic.

The current ARC101 validation does not establish closed-loop wheel-speed accuracy or
measured physical stop confirmation because encoder sensors are not installed.
Those properties remain future work for the final drive platform.
