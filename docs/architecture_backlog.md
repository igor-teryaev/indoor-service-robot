# Architecture backlog

- Keep hard actuator limits in the low-level motor controller.
- Implement independent STM32-side motion-command timeout and safety enforcement before powered operation.
- Define stale-command protection across control handover, recovery, queues, and future Pi-to-STM32 communication.
- Define motor stop completion semantics for the real motor adapter: `Success` must mean a confirmed safe stop, not merely that a stop request was queued or transmitted.
- Route runtime control and safety mutations through a single serialized owner/event loop before introducing callbacks or concurrency.