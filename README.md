# Indoor Service Robot

Autonomous indoor service robot with STM32-based motion control and Raspberry Pi vision.

## Goal

Build a mobile indoor robot capable of autonomous navigation, visual object detection, and interaction with household objects.

The first target use case is locating and retrieving slippers.

## Current status

The project is currently in the control-core development stage.

The Raspberry Pi-side C++ control architecture is implemented and covered by
host-side tests. It currently includes control-authority arbitration, safety
state handling, coordinated stopping and recovery, motion-command timeout
handling, differential-drive kinematics, and the wheel-level motor interface.

STM32 motor firmware, ELRS integration, Raspberry Pi vision, autonomous
navigation, and the Pi-to-STM32 communication protocol are planned integration
stages and are not yet implemented in this repository.

## Architecture

The system is split into two main controllers:

- Raspberry Pi — computer vision, high-level navigation, autonomy, Wi-Fi communication, and the main C++ application.
- STM32F446RE — motor control, wheel encoders, motion safety, low-level sensors, and manual ELRS control.

Manual control is provided through a RadioMaster Pocket using ExpressLRS.

## Control priority

1. E-STOP and hardware faults
2. Manual ELRS operator control
3. Raspberry Pi autonomous control

## Safety principles

- Motion commands have a limited validity period.
- Loss of the active control source causes a safe stop.
- Restoring communication does not automatically resume an old motion command.
- Fault recovery returns the robot to an idle state.
- Safety recovery requires a confirmed motor stop before the corresponding
  safety condition can be cleared.
- Recovery never automatically resumes a previous motion command.
- A new valid motion command is required before movement resumes.

## Hardware

Currently available or ordered:

- STM32 NUCLEO-F446RE
- Raspberry Pi
- Raspberry Pi camera
- RadioMaster Pocket ELRS
- 2 × DFRobot FIT0403 12 V geared motors with encoders
- DFRobot DRI0041 dual-channel motor driver

See `docs/hardware.md` for hardware details.

## Repository layout

- `firmware/stm32` — STM32 firmware
- `software/rpi` — Raspberry Pi application
- `protocol` — communication protocol definitions
- `software/rpi/tests` — Raspberry Pi control-core host tests
- `tests` — future cross-component and integration tests
- `tools` — development and diagnostic utilities
- `docs` — architecture and hardware documentation

## Raspberry Pi host build and tests

The Raspberry Pi control core is currently developed and tested on the host
using C++20, CMake, Ninja, and GoogleTest.

Configure:

```powershell
cmake -S . -B build -G Ninja
```

Build:

```powershell
cmake --build build
```

Run tests:

```powershell
ctest --test-dir build --output-on-failure
```

To build without tests:

```powershell
cmake -S . -B build-no-tests -G Ninja -DBUILD_TESTING=OFF
cmake --build build-no-tests
```

Current implemented host-side control components include:

- control authority arbitration;
- independent E-stop and hardware-fault state;
- coordinated safety stopping and recovery;
- motion watchdog;
- chassis-level `MotionCommand`;
- differential-drive kinematics;
- wheel-level `IMotorController` boundary.