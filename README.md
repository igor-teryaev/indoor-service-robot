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

The project currently has a host-tested control and communication core.

Implemented:

- Raspberry Pi-side control arbitration and differential-drive motion logic;
- shared C99 UART protocol;
- CRC16/CCITT-FALSE framing and streaming frame decoder;
- link synchronization and heartbeat messages;
- wheel-velocity protocol;
- reliable motion lifecycle commands with ACK and terminal responses;
- motion session lifecycle and retry handling;
- protection against stale asynchronous operation completions;
- wire-to-lifecycle and lifecycle-to-frame adapters.

The current host test suite passes **192/192 tests**.

STM32 motor firmware, live Raspberry Pi ↔ STM32 transport, encoder/PID control, ELRS input, computer vision, navigation, and real-hardware validation are still in progress.

Host tests validate software behavior only. Physical motor stopping and hardware communication have not yet been validated.

For detailed design information see:

- [System architecture](docs/architecture.md)
- [Architecture backlog](docs/architecture_backlog.md)

## Safety principles

- Motion commands have a limited validity period.
- Loss of valid motion control causes a safe stop.
- Restoring communication does not automatically resume old motion.
- A new valid motion command is required before movement resumes.
- Lifecycle ACK means that a command was accepted for processing, not that physical execution has completed.
- Successful stop completion must mean that the motors are physically confirmed stopped.

## Hardware

Current target hardware:

- STM32 NUCLEO-F446RE
- Raspberry Pi
- Raspberry Pi camera
- RadioMaster Pocket ELRS
- 2 × DFRobot FIT0403 12 V geared motors with encoders
- DFRobot DRI0041 dual-channel motor driver

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
