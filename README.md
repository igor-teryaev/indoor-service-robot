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
- STM32 TIM8/DRV8833 motor control;
- STM32 session-aware wheel-command gating and 250 ms motion-command watchdog;
- independent link heartbeat and motion-watchdog semantics: heartbeats keep the link alive but do not sustain motor motion;
- separate Raspberry Pi hardware-probe executable using the production `Stm32ClientRunner`.

The latest full WSL test checkpoint passes **342/342 tests**.

Hardware validation on Raspberry Pi 5 + NUCLEO-F446RE + DRV8833 + ARC101 confirmed:

- LINK_SYNC and heartbeat operation;
- fresh wheel demand starts a motion session and physically starts the motors;
- a one-shot command becomes stale and causes motion to stop;
- periodically refreshed wheel demand sustains continuous motion;
- an explicit zero command ends the motion session and physically stops the motors;
- pressing Nucleo RESET during active motion immediately stops the motors;
- the Raspberry Pi detects the unsynchronized STM32, reconnects, and does not replay the previous motion demand after resynchronization.

ARC101 currently has no encoder sensors installed, so wheel velocity is open-loop feed-forward rather than closed-loop velocity control.

Encoder/PID control for the final drive platform, ELRS runtime integration, computer vision, and navigation remain in progress.

For detailed design information see:

- [System architecture](docs/architecture.md)
- [Architecture backlog](docs/architecture_backlog.md)

## Safety principles

- Motion commands have a limited validity period.
- Loss of valid motion control causes a safe stop.
- Restoring communication does not automatically resume old motion.
- A new valid motion command is required before movement resumes.
- Lifecycle ACK means that a command was accepted for processing, not that physical execution has completed.
- On platforms with wheel feedback, successful stop completion should require physical confirmation that the wheels have stopped. The current ARC101 bring-up platform cannot provide that confirmation because encoder sensors are not installed.

## Hardware

Current target hardware:

- STM32 NUCLEO-F446RE
- ARC101 platform with DRV8833 for bring-up
- Rowenta RR6825WH drive base as the target chassis
- Raspberry Pi with camera
- RadioMaster Pocket ELRS

See [Hardware](docs/hardware.md) for details.

## Repository layout

- `firmware/stm32` — STM32 firmware
- `software/rpi` — Raspberry Pi C++ software
- `protocol` — shared C99 communication protocol
- `tests` — cross-component integration tests
- `tools` — development and diagnostic utilities
- `docs` — architecture and hardware documentation

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
