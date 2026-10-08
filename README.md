# Indoor Service Robot

Indoor mobile service robot built around a Raspberry Pi and STM32.

The first target use case is locating and retrieving slippers inside an apartment.

## Goal

Build a robot capable of:

- indoor navigation;
- visual object detection;
- manual operator control;
- autonomous movement;
- interaction with household objects.

## Architecture

The system is split between two controllers:

- **Raspberry Pi** — computer vision, navigation, autonomous behavior, manual/autonomous control arbitration, and high-level motion commands.
- **STM32F446RE** — deterministic motor control, wheel encoders, low-level motion safety, and hardware-facing sensors.

Both manual and autonomous control pass through the Raspberry Pi:

```text
Manual control / ELRS ─┐
                       ├──> Raspberry Pi
Autonomous control ────┘        │
                                │ UART
                                ▼
                              STM32
                                │
                         motors / encoders
```

The STM32 does not need to know whether a motion command originated from the operator or autonomous software.

Manual control has priority over autonomous control. Physical safety conditions such as E-STOP and hardware faults have priority over both.

## Current status

The project now has a host-tested and hardware-validated Raspberry Pi ↔ STM32 motion-control runtime.

Implemented:

- Raspberry Pi-side control arbitration and differential-drive motion logic;
- shared C99 UART protocol with CRC16/CCITT-FALSE framing and streaming decoding;
- Linux serial transport with nonblocking reads/writes, PTY-tested fragmentation, CRC recovery, disconnect handling, and bounded frame transmission;
- Raspberry Pi link synchronization, heartbeat scheduling/correlation, link timeout, disconnect, and reconnect handling;
- reliable motion lifecycle commands with ACK, terminal responses, retries, and exact transaction reuse;
- explicit Raspberry Pi motion lifecycle state: inactive, start pending, active, and end pending;
- demand-driven motion sessions: synchronization alone does not enable motion;
- latest-value-wins wheel demand with a 50 ms transmit period and 200 ms application-command freshness limit;
- explicit zero or stale wheel demand ends the active motion session;
- reconnect invalidates old wheel demand and never automatically replays previous movement;
- STM32 TIM8 motor control through the DFRobot DRI0041 for the Rowenta drivetrain;
- DRV8833 support retained separately for the earlier ARC101 bring-up platform;
- STM32 wheel-encoder input using PA6/TIM3_CH1 for the left wheel and PB6/TIM4_CH1 for the right wheel;
- encoder-confirmed physical-stop verification with a 200 ms settle interval and 1000 ms overall timeout;
- STM32 session-aware wheel-command gating and 250 ms motion-command watchdog;
- independent link heartbeat and motion-watchdog semantics: heartbeats keep the link alive but do not sustain motor motion;
- separate Raspberry Pi hardware-probe executable using the production `Stm32ClientRunner`.

The latest full WSL test checkpoint passes **361/361 tests**.

Hardware validation on Raspberry Pi 5 + NUCLEO-F446RE + DFRobot DRI0041 + Rowenta RR6825WH drivetrain confirmed:

- LINK_SYNC and heartbeat operation;
- fresh wheel demand starts a motion session and physically starts both wheels;
- periodically refreshed wheel demand sustains motion;
- explicit zero ends the active motion session and physically stops both wheels;
- both wheel encoders are counted independently by STM32 hardware timers;
- provisional encoder calibration is approximately 1000 counts per wheel revolution on both wheels;
- START and END lifecycle operations complete only after encoder-confirmed physical stop;
- continued encoder movement during physical-stop confirmation causes the lifecycle operation to fail after the 1000 ms timeout instead of falsely reporting success;
- lifecycle retries while physical-stop confirmation is pending do not restart the physical-stop deadline;
- reset cancels pending physical-stop confirmation state;
- the previously validated reconnect path does not replay stale motion demand after resynchronization.

Wheel velocity is still open-loop feed-forward. Encoder feedback is currently used for physical-stop confirmation; measured wheel-speed estimation is the next drivetrain milestone. Closed-loop velocity control/PID, ELRS runtime integration, computer vision, and navigation remain future work.

A separate electrical observation remains unresolved: manually back-driving a wheel while DRI0041 motor power is connected can disrupt the STM32/Pi link. With DRI0041 motor power disconnected, encoder counting and UART heartbeats remain stable.

For detailed design information see:

- [System architecture](docs/architecture.md)
- [Architecture backlog](docs/architecture_backlog.md)

## Safety principles

- Motion commands have a limited validity period.
- Loss of valid motion control causes a safe stop.
- Restoring communication does not automatically resume old motion.
- A new valid motion command is required before movement resumes.
- Lifecycle ACK means that a command was accepted for processing, not that physical execution has completed.
- Successful lifecycle stop completion requires encoder-confirmed physical stop.
- The physical-stop verifier requires 200 ms with no observed encoder movement and enforces a hard 1000 ms overall deadline.
- Heartbeats maintain communication state only and do not refresh the independent motion-command watchdog.

## Hardware

Current target hardware:

- STM32 NUCLEO-F446RE
- Rowenta RR6825WH drive base
- DFRobot DRI0041 dual motor driver
- Rowenta wheel encoders connected to TIM3 and TIM4
- Raspberry Pi 5 with camera
- RadioMaster Pocket ELRS

The ARC101 + DRV8833 setup is retained as the earlier drivetrain bring-up platform but is no longer the active target drivetrain.

See [Hardware](docs/hardware.md) for details.

## Repository layout

- `firmware/stm32` — STM32 firmware
- `software/rpi` — Raspberry Pi C++ software
- `protocol` — shared C99 communication protocol
- `tests` — cross-component integration tests
- `tools` — development and diagnostic utilities

## Build and test

Requirements:

- C99 / C++20 toolchain
- CMake 3.24+
- Ninja

Configure and build:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

Run tests:

```powershell
ctest --test-dir build --output-on-failure
```

Build without tests:

```powershell
cmake -S . -B build-no-tests -G Ninja -DBUILD_TESTING=OFF
cmake --build build-no-tests
```
