# STM32G474 Rev1 bring-up firmware

This is the STM32CubeMX/CMake project for the AgriGantry Rev1 controller based
on an STM32G474. It is intentionally separate from the Arduino Mega
PlatformIO project in `Firmware/` so neither controller can be built or
flashed by accident.

## Current scope

The Gabriel branch builds Test 15 for one supervised X-axis shaft revolution commanded by the Pi. A single STEP train goes to U4/X1 and U5/X2. Both drivers participate electrically. The bench has one motor and one encoder connected at X1. The known-good Test 14 recovery folder in the dedicated bring-up project is `Test_Baselines/Test14_PiX1Revolution_Pass`. Its ELF SHA-256 is `B642ECFAABB45DAFB4AC1C729F6C937B3497343B2F332CEB48795081A396473A`.

## Build

Install CMake 3.22 or newer, Ninja, and the Arm GNU Toolchain
(`arm-none-eabi-gcc`). From this directory:

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Build output is written to `build/Debug/` and is intentionally ignored by Git.
For Test 15 the laptop built the ELF, and OpenOCD on the Pi programmed and verified it through USB ST-LINK/V2 and JP1 SWD. [Pi-hosted programming and dashboard procedure](../../Webapp-Pi/webapp/README.md#run-the-supervised-x-axis-test)

## Layout

| Path | Purpose |
| --- | --- |
| `Core/` | Application code and STM32CubeMX-generated peripheral code. |
| `Drivers/` | Versioned STM32 HAL and CMSIS dependencies required to build. |
| `cmake/` | CubeMX CMake integration and Arm toolchain configuration. |
| `AgriGantry_Rev1_Bringup.ioc` | STM32CubeMX pin/peripheral configuration. |
| `CMakePresets.json` | Debug and Release CMake build presets. |

## Test 15 configuration and shutdown

The CubeMX configuration and generated peripheral code come from the verified Test 14 baseline. The Fusion schematic and routed board (`Control_Board_Simulation.fsch` and `.fbrd`) show the following connections. The existing pin assignments cover the test.

| Connection | Both X drivers | U4/X1 | U5/X2 |
| --- | --- | --- | --- |
| Motion and power control | PE3 STEP, PE2 DIR, PE6 nSLEEP, PF9 enable through the external MOTION_OK AND gate | PE4 chip select, PF10 nFAULT | PE5 chip select, PC13 nFAULT |
| Reference and feedback | VREF connected to PGND on each IC | TIM5 on PA0/B and PA1/A, PA2 index | Motor connector empty in this test |

SPI3 uses PC10/11/12. The Pi uses USART1 on PC4/5 at 115200 8N1. Boot initializes GPIO, SPI3, TIM5, USART1 and the encoder. ENA and sleep start LOW, STEP starts LOW, chip selects start HIGH and D5 is off in idle. The MCU has no separate measurement of the external MOTION_OK level.

1. An explicit paired move keeps the shared enable LOW, holds nSLEEP LOW for at least 1 ms, wakes only X and waits at least 30 ms. Both nFAULT inputs and the settled FAULT and DIAG2 registers must pass.
2. With a separate chip select per driver, the firmware checks disabled CTRL1, writes and verifies `CTRL1=0x0F`, `CTRL13[0x10]=0x12` and `CTRL11[0x0E]=0x19`, and checks `CTRL2=0x06`. The internal 3.3 V reference and torque DAC produce about 0.30 A nominal full-scale **phase current**. The bench PSU displays combined input current at 24 V.
3. With PF9 still LOW, each chip must read back `CTRL1=0x8F`. The MCU then raises PF9. U9 also requires the external MOTION_OK signal. These enable conditions and the default-disabled open-load detection are documented in the TI DRV8461SPWPR data sheet, table 7-29 and the CTRL9, CTRL11 and CTRL13 register descriptions. The test uses the reset setting for open-load detection and stops on a reported open-load flag. [TI DRV8461 data sheet](https://www.ti.com/lit/ds/symlink/drv8461.pdf)
4. After 100 ms of alignment, the STM32 captures the starting encoder and index counts. It issues 3,200 rising STEP edges in 1/16-step mode. The first and last 400 pulses ramp around 2,400 cruise pulses. STEP is nominally HIGH for 2 ms and LOW for 10 to 1 to 10 ms, plus software and diagnostic overhead. D5 is solid during the pulse train. Both nFAULT pins and the stop input are checked during alignment and around each pulse. Independent FAULT and DIAG2 reads repeat every 100 pulses. A fault, malformed SPI response, HAL error, temperature warning/shutdown, stop or lost supervision ends the test.
5. The STM32 captures final counts, drops PF9, STEP, DIR and sleep, deselects the chips and turns D5 off before it reports the result. Y and Z stay disabled. The result includes the actual number of rising edges, signed X1 encoder delta, index delta and separate last driver readbacks. The enable readbacks describe the enabled stage of the test. They remain available for inspection after shutdown.

## Pi command and result

The Pi owns COMM during a run. Its idle status polls and scans pause while the move is in progress. Test 15 advertises itself through the read-only `PAIR?` command. The phone's **Arm X** reserves the test locally for 30 seconds. **Run one revolution** makes the Pi verify readiness, send `ARM X`, wait for its acknowledgement and send `MOVE X` once. The STM32 accepts one attempt per boot across the paired and original X1-only protocols.

During motion the Pi sends a `~` supervision byte approximately every 200–250 ms. The firmware checks expiry at 1,200 ms. **Stop X**, leaving the controls, or loss of operator presence requests a stop. A broken UART or a stopped Pi cuts off the supervision bytes. Reconnecting retrieves `RESULT` without replaying a move. Normal `PING`, `STATUS`, `SCAN` and the existing `ARM X1` / `MOVE X1` scope remain available.

The paired reply contains one `RESULT X` line, then `DRV X1` and `DRV X2`. Each driver row reports configuration checks, last CTRL1, diagnostic sample count, raw FAULT and DIAG2 frames and the last SPI register, HAL status and raw response. A verified revolution requires 3,200 issued pulses, an absolute X1 delta of 3,800–4,200 counts, exactly one Z index pulse and valid diagnostics from both chips. The sign identifies the encoder direction. U5's register readbacks cover its command path. Mechanical travel, temperature under sustained load and a second shaft remain separate measurements.

## November 24 build and programming results

The Debug preset completed a clean ARM build with 35 build steps, zero compiler/linker warnings or errors, 31,560 bytes of flash and 2,880 bytes of RAM. Output is `build/Debug/AgriGantry_Rev1_Bringup.elf`. ELF SHA-256 is `45C37253DBCB93A5152D5F4F50DFA46C4D486723FE2E0A0D0E7F234CDFB8372E`.

Twelve native tests compiled the actual motor and UART C files against modeled hardware I/O. They exercised one full trajectory, refused preconditions, X2 register and enable readback mismatches, X2 SPI timeout or malformed header, thermal and open-load reports, X2 nFAULT, operator stop and missing supervision. A failed X2 enable readback produced zero pulses even after U4 accepted EN_OUT. The Python dashboard suite passed 22 tests. The Pi parser also decoded ten result transcripts from the compiled C fixture.

**Test 15 programming passed.** Gabriel reported that the Pi-hosted OpenOCD write, verification and STM32 reset completed successfully over the existing USB ST-LINK/V2 and JP1 SWD wiring. He also reported the recurring adapter-speed message. The earlier recorded run on this probe requested 2,000 kHz and selected 1,800 kHz. OpenOCD reports its selection when setting the SWD clock, which can happen again during reset. Verification compares the programmed flash contents with the image. [Dashboard instructions](../../Webapp-Pi/webapp/README.md#run-the-supervised-x-axis-test)

## Test 15 hardware result

**PASS on November 24, 2026.** Gabriel commanded one X-axis revolution from the phone and observed X1 rotate. The phone screenshot and the Pi dashboard's retained result agree on `DONE`, 3,200 issued STEP pulses, an X1 encoder change of −3,998 counts and one index pulse. The magnitude was two counts below the nominal 4,000. The dashboard status endpoint on port 5001 also returned `phase=finished`, `verified=true` and an STM32-reported motion window of 19,856 ms.

| Driver | Bench connection | Configuration | Enabled CTRL1 readback | Diagnostic samples | Last raw FAULT / DIAG2 | Last SPI transaction |
| --- | --- | --- | --- | --- | --- | --- |
| U4 / X1 | Motor and encoder connected | Verified | `0x8F` | 34 | `0xC000` / `0xC000` | Register `0x02`, HAL `0`, raw `0xC000` |
| U5 / X2 | Motor socket empty | Verified | `0x8F` | 34 | `0xC000` / `0xC000` | Register `0x02`, HAL `0`, raw `0xC000` |

The raw frames contain status byte `0xC0` and register data `0x00`. Both drivers completed the checks without a reported fault or thermal warning. Post-run telemetry reply 235 showed X/Y/Z enable commands `0/0/0`, the live X1 counter at −4,013 and uptime 551,320 ms. The live counter includes motion outside the measured window. The −3,998 result is the difference between the count captured after alignment and the count just before disabling the drivers.

Gabriel observed D5 off after the run. The firmware commands PD12 HIGH immediately before the first pulse and LOW during shutdown. The recorded 19,856 ms brackets those commands. There was no optical observation during motion or recorded PD12 waveform. A later SWD read of GPIOD's output data register (`0x48000C14`, bit 12) gives the current output latch only. [Pulse train and D5 command](Core/Src/motor_test.c#L643)
