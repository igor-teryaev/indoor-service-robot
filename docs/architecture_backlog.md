# Architecture backlog

## Completed host-side checkpoint

Through `a99f455`, the repository implements framing and ingress routing, the
motion reliable receiver, lifecycle coordination with local operation IDs, and
wire/lifecycle adapters. A fresh host build passed **192/192 CTest tests** on
2026-09-17. See [System architecture](architecture.md) for the implemented
contracts and their runtime boundaries.

## Next: STM32 physical stop integration

- Implement the nonblocking STM32 motor/safety adapter for `ENSURE_STOPPED`.
  Capture its `operation_id` and return that same ID with `SUCCESS` or `FAILED`;
  superseding a pending transaction must not restart the same physical stop.
- Define and implement the physical evidence required for stop completion.
  `SUCCESS` must mean a confirmed safe stop, not merely a queued or transmitted
  request. Define stop deadlines and failure handling; clearing the protocol
  session after `STOP_FAILED` must not remove motor-level fault inhibition.
- Implement independent STM32-side motion-command timeout and safety enforcement
  before powered operation. Keep hard actuator limits in the low-level controller.

## Runtime and transport integration

- Connect receive bytes -> decoder -> ingress router -> handler, dispatch lifecycle
  actions, and transmit encoded ACK/response frames over the chosen Pi-to-STM32
  transport. Integrate sender retries and timeouts with the receiver contract.
- Connect link synchronization and heartbeat messages to runtime link state and
  link-loss detection. Define reset/reconnect behavior: coordinator `reset()`
  invalidates logical state but does not itself stop motors.
- Gate session-tagged wheel commands by lifecycle state, safety, control authority,
  and freshness. Complete stale-command protection across handover, recovery,
  queues, and reconnects; link restoration must require a fresh movement command.
- Route runtime control, safety, protocol transactions, and asynchronous completion
  events through a single serialized owner/event loop before introducing callbacks
  or concurrency. Use runtime `reset()` rather than reinitialization while old
  completions can still arrive.

## Hardware validation

- Validate stop success/failure, stop timeout, link loss, and recovery on STM32
  hardware. Verify that late completions cannot complete a newer operation and
  that retries/supersede do not duplicate the physical stop operation.
- Measure timing and confirm independent low-level safety under Raspberry Pi or
  transport failure. Host-test results do not establish physical safety behavior.
