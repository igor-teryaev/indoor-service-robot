# Architecture backlog

## Completed motion-runtime and physical-stop checkpoint

The Raspberry Pi ↔ STM32 motion-control runtime is integrated and validated on
real hardware. The Rowenta drivetrain and wheel-feedback path are now the active
low-level motion platform.

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
- prevention of automatic motion replay after reconnection;
- Rowenta RR6825WH motor control through the DFRobot DRI0041;
- independent left/right wheel encoder counting using TIM3 and TIM4;
- encoder-confirmed lifecycle physical stop;
- 200 ms physical-stop settle interval;
- hard 1000 ms physical-stop deadline;
- timeout precedence over settlement at the deadline boundary;
- physical-stop timing anchored after the motor stop command succeeds;
- duplicate lifecycle retries without restarting the physical-stop operation;
- reset cancellation of pending physical-stop confirmation.

The full WSL test checkpoint passes **361/361 tests**.

Hardware validation on Raspberry Pi 5 + NUCLEO-F446RE + DFRobot DRI0041 +
Rowenta RR6825WH confirmed:

- motion start from fresh demand;
- sustained motion from refreshed demand;
- explicit-zero physical stop;
- independent left/right encoder counting;
- approximately 1000 encoder counts per wheel revolution on both wheels;
- START and END completion only after encoder-confirmed physical stop;
- terminal stop failure when encoder movement continues beyond the physical-stop timeout;
- reset/reconnect behavior without replaying previous movement.

## Remaining low-level motion work

Next milestone:

- implement measured wheel-speed estimation from encoder counts;
- calibrate counts per wheel revolution more carefully;
- measure effective wheel circumference;
- define sampling interval and 16-bit counter-wrap handling;
- characterize low-speed quantization and zero-speed behavior;
- validate measured speed for both wheels and both directions;
- characterize startup and sustain thresholds under the Rowenta drivetrain;
- characterize left/right speed mismatch and stopping behavior;
- characterize reversal behavior and define a conservative operating envelope.

Later:

- implement closed-loop wheel-speed control after measurement is validated;
- define encoder-health/fault detection beyond simple absence of observed pulses;
- decide whether physical-stop failure should latch a persistent motor/safety fault;
- preserve motor-level fault inhibition independently of logical protocol-session cleanup;
- measure watchdog, transport-failure, reconnect, and physical-stop timing margins under a wider range of operating conditions.

A separate electrical issue remains open:

- manually back-driving a wheel while DRI0041 motor power is connected can disrupt
  STM32/Pi communication;
- with DRI0041 motor power disconnected, encoder counting and UART heartbeat
  communication remain stable;
- investigate whether the disturbance is caused by motor regeneration, supply/ground
  disturbance, or another electrical effect before relying on powered back-drive or
  aggressive reversal tests.

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

Completed on Raspberry Pi 5 + NUCLEO-F446RE + DFRobot DRI0041 +
Rowenta RR6825WH:

- live Raspberry Pi ↔ STM32 synchronization and heartbeat operation;
- demand-driven motion start on fresh non-zero wheel demand;
- sustained motion while application demand is refreshed;
- explicit-zero motion stop;
- independent left/right encoder counting;
- provisional encoder calibration near 1000 counts per wheel revolution;
- encoder-confirmed START and END lifecycle completion;
- encoder movement preventing false physical-stop completion;
- terminal `MOTION_RESPONSE_STOP_FAILED` after physical-stop timeout;
- lifecycle retry while physical stop is pending without restarting its deadline;
- reset cancellation of pending physical-stop confirmation;
- STM32 reset during active motion;
- Raspberry Pi detection of the resulting unsynchronized link;
- automatic reconnect and resynchronization;
- verification that previous wheel demand is not replayed after reconnect.

Still required:

- validate measured wheel-speed estimation against independently timed wheel motion;
- characterize low-speed measurement limits;
- characterize powered forward/reverse operation on both wheels;
- define the supported Rowenta command/speed envelope before closed-loop control;
- investigate the powered back-drive communication disturbance;
- measure watchdog, transport-failure, reconnect, and physical-stop timing margins;
- validate independent low-level safety under Raspberry Pi failure or prolonged
  transport failure.