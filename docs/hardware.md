# Hardware

## Current hardware

- STM32 NUCLEO-F446RE
- Raspberry Pi 5 4GB
- Raspberry Pi camera
- RadioMaster Pocket ELRS transmitter
- DRV8833 dual H-bridge motor driver
- ARC101 temporary differential-drive chassis
- Rowenta RR6825WH drive base planned as the final chassis

## Planned / not yet integrated

- ExpressLRS 2.4 GHz receiver with CRSF/UART output
- final battery and power-distribution design
- 5 V DC/DC supply for Raspberry Pi
- obstacle/range sensors
- slipper pickup mechanism
- encoder feedback for closed-loop wheel-speed control

## Drive

The robot uses differential drive with two independently controlled motors.

The current ARC101 bring-up platform is driven through the DRV8833 from the STM32. The ARC101 encoder disks are present, but encoder sensors are not installed, so the current `WHEEL_VELOCITY` path is open-loop feed-forward rather than measured closed-loop velocity control.

On ARC101, the motors physically require approximately motor command 800 to start and can sustain motion at approximately 750.

The intended final chassis is the Rowenta RR6825WH. Its suspended drive units and optical feedback are expected to support a later closed-loop implementation, but that work has not yet been integrated.

## Power architecture

Planned topology:

Battery
  |
  +---- motor driver ---- left/right motors
  |
  +---- DC/DC 5 V ------- Raspberry Pi
  |
  +---- low-voltage electronics

Motor and logic power distribution, protection, fusing, and battery selection will be finalized after the complete power budget is known.
