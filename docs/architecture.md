# System Architecture

The robot is split into a high-level Linux computer and a deterministic low-level controller.

## High-level controller

Raspberry Pi responsibilities:

- camera acquisition and computer vision;
- object detection and localization;
- high-level navigation and behavior;
- autonomous motion commands;
- Wi-Fi communication with the operator PC.

The main application will be developed primarily in C++.

## Low-level controller

STM32F446RE responsibilities:

- motor control;
- wheel encoder processing;
- velocity control;
- motion state machine;
- command timeout and fail-safe stop;
- hardware fault handling;
- low-level sensors;
- ELRS/CRSF manual control.

## Communication

Planned internal architecture:

```text
RadioMaster Pocket
        |
      ELRS
        |
 ELRS receiver
        |
   CRSF / UART
        |
      STM32
        |
   UART / CAN
        |
 Raspberry Pi
        |
 Camera / Wi-Fi
Control priority
E-STOP / hardware fault
        ↓
manual ELRS control
        ↓
Raspberry Pi autonomous control
```

Safety principles
- Motion commands have a bounded validity period.
- Loss of an active control source results in a safe stop.
- Restoring communication never resumes a stale motion command automatically.
- Fault recovery returns the robot to an idle state; a new motion command is required to move again.
- STM32 remains responsible for low-level motion safety even if the Raspberry Pi application fails.