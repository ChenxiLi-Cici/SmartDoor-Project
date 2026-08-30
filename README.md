# DESN2000 Smart Door

Embedded smart-door controller developed for DESN2000 using an STM32 NUCLEO-F303RE board.

The system combines card-based entry authorisation, free exit, LDR passage sensing, tailgating detection, safe door closing, scheduled access, an administrator menu, LCD feedback, LEDs, a buzzer, a keypad, and stepper-motor control.

## Hardware and tools

- STM32 NUCLEO-F303RE (`STM32F303RETx`)
- STM32CubeIDE / STM32 HAL
- Two LDR passage sensors
- NFC card reader
- Keypad and character LCD
- Stepper motor, LEDs, and buzzer

## Repository layout

- `SmartDoor/Core/Inc/` — application and peripheral headers
- `SmartDoor/Core/Src/` — FSM, sensor, scheduler, UI, and actuator implementation
- `SmartDoor/Drivers/` — STM32 HAL and CMSIS drivers
- `SmartDoor/SmartDoor.ioc` — STM32CubeMX configuration
- `docs/testing/` — LDR/FSM timing analysis and regression-test matrix

## Build

1. Open STM32CubeIDE.
2. Import `SmartDoor` as an existing STM32CubeIDE project.
3. Select the `Debug` configuration and run a clean build.
4. Flash the generated firmware to the NUCLEO-F303RE board.

Generated `Debug` and `Release` files are intentionally excluded from version control.

## Testing

See the [complete LDR/FSM modification, timing, and regression-test report](docs/testing/LDR_FSM_%E5%AE%8C%E6%95%B4%E4%BF%AE%E6%94%B9%E4%B8%8E%E6%97%B6%E5%BA%8F%E6%8A%A5%E5%91%8A_2026-08-07.md).

Release source baseline: `remove` at commit `e22b4b8`. The LDR/FSM test logic originated from `FixLdr` and is included in this release history.

## Team

- Xiaolei Wu (`z5556281`)
- Leyi Yang (`z5535409`)
- Chenxi Li (`z5531794`)
