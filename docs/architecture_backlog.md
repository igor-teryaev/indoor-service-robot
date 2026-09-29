# Architecture backlog

## Completed motion-runtime checkpoint

The Raspberry Pi ↔ STM32 motion-control runtime is now integrated and validated on
real hardware.

Completed:

- live Linux serial transport between Raspberry Pi and STM32;
- link synchronization and heartbeat supervision;
- disconnect detection and automatic reconnect;
- bounded Raspberry Pi frame transmission with a 100 ms absolute deadline;
- reliable motion lifecycle START/END transactions with retry handling;
- explicit Raspberry Pi motion-session state;
- demand-driven motion-session creation;
- latest-value-wins wheel demand;
- 200 ms Raspberry Pi application-demand freshness limit;
- 50 ms wheel-command transmit period while demand remains fresh;
- explicit-zero and stale-demand motion termination;
- STM32 session-aware wheel-command gating;
- independent 250 ms STM32 motion-command watchdog;
- reset/reconnect invalidation of old motion state and wheel demand;
- prevention of automatic motion replay after reconnection.

The full WSL test checkpoint passes **342/342 tests**.

Hardware validation on Raspberry Pi 5 + NUCLEO-F446RE + DRV8833 + ARC101 confirmed
motion start, sustained fresh demand, stale-demand stop, explicit-zero stop, and
reset/reconnect behavior without replaying previous movement.

## Remaining low-level motion work

- Add encoder sensing to a drive platform that supports measured wheel feedback.
- Implement closed-loop wheel-speed control rather than ARC101 open-loop feed-forward.
- Define measured physical stop confirmation once usable wheel feedback is available.
- Define stop deadlines and fault handling for cases where a commanded stop cannot
  be physically confirmed.
- Preserve motor-level fault inhibition independently of logical protocol-session
  cleanup.
- Measure timing margins for watchdog, transport failure, and physical stopping on
  the final drive platform.

## Runtime and transport integration

Completed:

- receive bytes -> streaming decoder -> ingress routing -> link/motion handling;
- encoded ACK/response and wheel-frame transmission over the live Pi-to-STM32 UART;
- sender retry handling and bounded frame-transmission deadlines;
- link synchronization, heartbeat supervision, disconnect detection, and reconnect;
- motion lifecycle START/END retry handling;
- session-tagged wheel-command gating;
- latest-value-wins application wheel demand;
- stale-command protection across active motion and reconnect;
- reconnect invalidation of cached wheel demand so restored communication cannot
  replay movement automatically;
- serialized Raspberry Pi runtime ownership through cooperative
  `Stm32ClientRunner::poll()`.

Remaining runtime work:

- integrate manual ELRS/CRSF input;
- connect the existing control-authority arbitration to the live STM32 runner;
- integrate autonomous navigation output as another motion-demand source;
- define the final application-level interface above `Stm32ClientRunner`;
- evaluate whether the current cooperative polling structure needs further
  scheduling changes once camera, navigation, and manual-control workloads are active.

## Hardware validation

Completed on Raspberry Pi 5 + NUCLEO-F446RE + DRV8833 + ARC101:

- live Raspberry Pi ↔ STM32 synchronization and heartbeat operation;
- demand-driven motion start on fresh non-zero wheel demand;
- sustained motion while application demand is refreshed;
- stale-demand motion stop;
- explicit-zero motion stop;
- STM32 reset during active motion;
- Raspberry Pi detection of the resulting unsynchronized link;
- automatic reconnect and resynchronization;
- verification that previous wheel demand is not replayed after reconnect.

Still required:

- validate measured physical stop confirmation once encoder feedback is available;
- validate stop-failure and stop-timeout behavior against real feedback;
- verify late physical completions cannot affect a newer operation in the final
  asynchronous stop implementation;
- verify retries and supersede behavior do not duplicate a physical stop operation;
- measure watchdog, transport-failure, reconnect, and physical-stop timing margins;
- validate independent low-level safety under Raspberry Pi failure or prolonged
  transport failure;
- repeat the relevant safety tests on the final Rowenta drive platform.