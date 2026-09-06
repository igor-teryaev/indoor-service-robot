# Indoor Service Robot

Autonomous indoor service robot with STM32-based motion control and Raspberry Pi vision.

## Goal

Build a mobile indoor robot capable of autonomous navigation, visual object detection, and interaction with household objects.

The first target use case is locating and retrieving slippers.

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
- `tests` — host-side and integration tests
- `tools` — development and diagnostic utilities
- `docs` — architecture and hardware documentation

## Status

Initial repository structure and system architecture are defined.