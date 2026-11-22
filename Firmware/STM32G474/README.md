# STM32G474 Rev1 bring-up firmware

This is the STM32CubeMX/CMake project for the AgriGantry Rev1 controller based
on an STM32G474. It is intentionally separate from the Arduino Mega
PlatformIO project in `Firmware/` so neither controller can be built or
flashed by accident.

## Current scope

This snapshot preserves the working Rev1 bring-up project, including STM32 HAL
and CMSIS sources, CubeMX configuration, SPI motor-driver diagnostics, timer
and encoder diagnostics, interlocks, and motor-test code. It is the starting
point for the Raspberry Pi-to-STM32 communication work.

This firmware controls real hardware. Keep the mechanism clear, verify the
active test mode in `Core/Src/main.c`, and do not enable motor motion solely to
test communications.

## Build

Install CMake 3.22 or newer, Ninja, and the Arm GNU Toolchain
(`arm-none-eabi-gcc`). From this directory:

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Build output is written to `build/Debug/` and is intentionally ignored by Git.
Program the resulting image using the normal ST-Link/STM32CubeProgrammer
workflow for the Rev1 board.

## Layout

| Path | Purpose |
| --- | --- |
| `Core/` | Application code and STM32CubeMX-generated peripheral code. |
| `Drivers/` | Versioned STM32 HAL and CMSIS dependencies required to build. |
| `cmake/` | CubeMX CMake integration and Arm toolchain configuration. |
| `AgriGantry_Rev1_Bringup.ioc` | STM32CubeMX pin/peripheral configuration. |
| `CMakePresets.json` | Debug and Release CMake build presets. |

## Pi communication work

Start on a dedicated branch and validate a non-motion UART heartbeat/status
exchange before adding any command that can change motor state. The Raspberry
Pi service should be the one serial-port owner; the browser dashboard should
communicate with the Pi service rather than opening the STM32 link directly.
