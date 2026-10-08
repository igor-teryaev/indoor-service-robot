# Hardware

## Current hardware

- STM32 NUCLEO-F446RE
- Raspberry Pi 5 4GB
- Raspberry Pi camera
- RadioMaster Pocket ELRS transmitter
- Rowenta RR6825WH differential-drive base
- DFRobot DRI0041 dual motor driver
- original Rowenta 4S Li-ion battery, 14.8 V nominal
- left and right Rowenta optical wheel sensors

The earlier ARC101 + DRV8833 setup is retained as the original drivetrain
bring-up platform but is no longer the active target drivetrain.

## Planned / not yet integrated

- ExpressLRS 2.4 GHz receiver with CRSF/UART output
- dedicated 4S Li-ion charging stage for use with the original Rowenta dock
- 5 V DC/DC supply for Raspberry Pi
- final power-distribution and protection design
- obstacle/range sensors
- slipper pickup mechanism
- measured wheel-speed estimation
- closed-loop wheel-speed control

## Drive

The robot uses differential drive with two independently controlled Rowenta
RR6825WH wheel modules.

The active motor driver is the DFRobot DRI0041.

STM32 connections:

- PC6 / TIM8_CH1 -> ENA, left-wheel PWM
- PC7 -> IN1
- PC5 -> IN2
- PC8 / TIM8_CH3 -> ENB, right-wheel PWM
- PC9 -> IN3
- PB10 -> IN4

TIM8 currently runs at 10 kHz PWM.

The motor-driver boundary remains generic. DRI0041-specific electrical behavior
is implemented below the generic `motor_driver` API, while the older DRV8833
implementation remains isolated for the ARC101 platform.

At approximately 14.45 V battery voltage with unloaded wheels:

- both Rowenta motors start reliably at approximately 15% PWM;
- the observed start threshold is roughly 12-13% PWM;
- approximately 10% PWM is a practical reliable sustain level;
- approximately 7.5% PWM is marginal and can become unstable;
- the right wheel has shown somewhat faster stopping behavior than the left.

These values are bring-up observations rather than final calibrated operating
limits.

The current `WHEEL_VELOCITY` path remains open-loop feed-forward. Requested wheel
velocity is converted to motor command, but measured wheel speed is not yet used
to regulate the motor output.

## Wheel encoders

Each Rowenta wheel module contains a single-channel optical pulse sensor.

Connections:

- left encoder signal -> PA6 / TIM3_CH1
- right encoder signal -> PB6 / TIM4_CH1

Sensor wiring:

- brown -> 3.3 V through approximately 330 ohms
- black -> GND
- white -> timer input
- white -> 3.3 V through approximately 2.2 kohm pull-up

TIM3 and TIM4 use external-clock mode and count rising edges directly in hardware.
The counters are 16-bit.

Manual calibration produced:

- left wheel: 9974 counts over 10 revolutions, approximately 997.4 counts/rev;
- right wheel: 10953 counts over 11 revolutions, approximately 995.7 counts/rev.

For bring-up, approximately **1000 counts per wheel revolution** is used as a
provisional calibration.

The sensors are single-channel, so raw encoder feedback measures pulse-count
change and movement magnitude but does not directly provide rotation direction.

Encoder feedback is currently used for physical-stop confirmation. Measured
wheel-speed estimation is the next drivetrain milestone.

## Physical-stop feedback

Lifecycle START and END operations require encoder-confirmed physical stop.

After the motor stop command succeeds:

- encoder counts are monitored;
- no observed encoder movement for 200 ms is considered settled;
- a hard 1000 ms overall physical-stop deadline is enforced;
- continued encoder movement causes lifecycle stop failure rather than false
  success.

The 1000 ms deadline takes precedence over the 200 ms settle interval.

## Power architecture

Current bench drivetrain topology:

```text
Rowenta 4S battery
        |
        +---- DRI0041 ---- left/right Rowenta motors

Nucleo USB
        |
        +---- STM32 + DRI0041 logic reference

Rowenta encoders
        |
        +---- STM32 TIM3 / TIM4