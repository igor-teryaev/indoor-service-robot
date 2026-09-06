# Hardware

## Available or ordered

- STM32 NUCLEO-F446RE
- Raspberry Pi
- Raspberry Pi camera
- RadioMaster Pocket ELRS transmitter
- 2 × DFRobot FIT0403 12 V geared DC motor with encoder
- DFRobot DRI0041 dual-channel motor driver

## Planned

- ExpressLRS 2.4 GHz receiver with CRSF/UART output
- wheels and mechanical chassis
- battery
- 5 V DC/DC supply for Raspberry Pi
- obstacle/range sensors
- slipper pickup mechanism

## Drive

The robot uses differential drive with two independently controlled geared DC motors.

The FIT0403 motors provide quadrature encoder feedback. STM32 will use encoder measurements for closed-loop wheel-speed control rather than relying only on open-loop PWM duty cycle.

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
