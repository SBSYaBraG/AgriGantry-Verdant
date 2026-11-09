# AgriGantry firmware

This directory is the PlatformIO project for the Arduino Mega 2560. It has
three intentionally separate firmware targets. Select the PlatformIO
environment explicitly; opening a source file in VS Code does not determine
what gets built or uploaded.

## Choose the right firmware target

| Environment | Source file | Use it for | Do not use it for |
| --- | --- | --- | --- |
| `megaatmega2560` | `src/main.cpp` | The normal iCL AgriGantry program | Encoder bench tests or manual endpoint calibration. |
| `megaatmega2560-encoder-test` | `src/encoder_test_main.cpp` | A standalone 17HS19-2004-ME1K and TB6600/AM26LV32E test | Moving the gantry. |
| `megaatmega2560-manual-calibration` | `src/manual_calibration_main.cpp` | Current supervised iCL travel calibration and percentage-position testing | Automatic physical homing or unattended motion. |

The manual target builds only `manual_calibration_main.cpp`; it cannot link the
normal program or the encoder bench test. This separation is a deliberate
safety feature.

## Manual calibration: current physical workflow

Use `megaatmega2560-manual-calibration` for the installed iCL motor system.
At startup, every driver is disabled and every pulse line is low. The firmware
does not automatically home, jog, or drive toward a physical endpoint.

Manual calibration is an operator-assisted estimate:

1. Select one axis and start its calibration.
2. Hold its jog key only while moving toward a visually observed endpoint.
3. Release it, wait two seconds, and confirm the endpoint with Enter.
4. Jog in the reverse direction. The confirmed first endpoint is captured as
   software 0%, and each commanded pulse is counted.
5. Confirm the opposite endpoint to save the estimated 0-100% travel range.

After an axis is calibrated, free jogging is locked. It can make bounded
percentage moves, or return to its 50% midpoint. Each axis keeps its own
calibration while power remains on, so calibrating Y does not erase an already
calibrated X or Z. A Mega power cycle makes all stored ranges and positions
untrusted and requires a new manual reference.

`HOME <axis>` is a midpoint move to the stored 50% estimate. `HOME ALL`
returns X, then Y, then Z to their midpoints only when all three were
calibrated in the same powered session. Neither command searches for a
physical endpoint.

For the exact iCL pin map and implementation-level safety details, see the
[Motors_Manual guide](lib/Motors_Manual/README.md).

## Build, upload, and keyboard control

From this `Firmware` directory, close PlatformIO Serial Monitor before starting
the keyboard controller. Both programs cannot own the same `COM` port.

```powershell
pio run -e megaatmega2560-manual-calibration -t upload
python -m pip install -r scripts/requirements-manual.txt
python scripts/manual_keyboard_controller.py --port COMx
```

Replace `COMx` with the Mega's actual port. In the PlatformIO extension, use
the **Upload** task under `megaatmega2560-manual-calibration`, not the project
default target unless that target is the one selected.

### Keyboard controls

| Key | Action |
| --- | --- |
| `F1`, `F2`, `F3` | Select X, Y, or Z. |
| `C` | Begin calibration for the selected axis. |
| `A` / `D` | Hold to jog X during its calibration. |
| Left / Right Arrow | Hold to jog Y during its calibration. |
| `W` / `S` | Hold to jog Z during its calibration. |
| Enter | Confirm a released endpoint after the two-second confirmation delay. |
| `1` through `5` | Move the selected calibrated axis to 10%, 25%, 50%, 75%, or 90%. |
| `H` | Move the selected calibrated axis to 50%. |
| `G` | Request `HOME ALL` (X, then Y, then Z to their 50% midpoints). |
| Space | Controlled ramped stop. |
| `O` or Esc | Immediate latched emergency stop. |

An asserted ALM input, lost jog heartbeat, or emergency stop disables the
drivers. Correct the physical condition before using the protocol's `CLEAR`
command. Do not bypass an alarm simply to continue a test.

### What the current values mean

`travel_steps`, percentage targets, and reported positions are counts of
commanded Mega step pulses. They include the pulses issued during acceleration
and deceleration, but they do not prove that the mechanism physically moved
that far. Use tape/reference marks and supervision for current repeatability
tests. The later encoder implementation will log command-versus-feedback error
separately.

## Encoder bench test

The encoder bench target is intentionally isolated from the gantry and uses
the replacement 17HS19-2004-ME1K motor, a TB6600, and an AM26LV32E differential
receiver. It provides quadrature/index counts and test logs; it is not a
closed-loop controller.

Follow the full [Motors_Enc bench-test guide](lib/Motors_Enc/README.md) before
wiring or uploading that target.

## Project layout

| Path | Role |
| --- | --- |
| `src/main.cpp` | Normal iCL program entry point. |
| `src/manual_calibration_main.cpp` | Safety-first manual-calibration entry point. |
| `src/encoder_test_main.cpp` | Standalone encoder bench-test entry point. |
| `lib/Motors_Manual/` | Calibration state machine, pulse motion, safety, and serial protocol. |
| `lib/Motors_ICL/` | Current normal iCL motor-control library. |
| `lib/Motors_Enc/` | Encoder bench-test support and documentation. |
| `scripts/manual_keyboard_controller.py` | Desktop keyboard-to-serial controller for manual calibration. |
| `scripts/export_encoder_readings.ps1` | Exports encoder-test serial summaries to dated CSV files. |
| `logs/` | Live PlatformIO monitor captures. The encoder exporter reads the newest capture here. |
| `../data/encoder/arduino_exports/` | Versioned CSV summaries created from encoder monitor captures. |
| `CHANGELOG.md` | Dated hardware, testing, and firmware history. |

## Pi and dashboard boundary

The Pi is planned to own serial-session management, scheduled endurance tests,
and long-term logs. The Mega remains the only component that generates step
pulses and enforces motion safety. A browser must call the Pi service; it must
never directly open the Mega serial port or generate a trajectory.

The design contract and planned web-app interface are in
[`../Webapp-Pi/MANUAL_CALIBRATION_INTEGRATION.md`](../Webapp-Pi/MANUAL_CALIBRATION_INTEGRATION.md).
The current `Webapp-Pi/webapp` implementation remains a simulated dashboard until
that service exists and has been validated.

## Record changes

For any change that affects a pin map, motion behavior, test procedure, or
hardware safety, add a dated note to [CHANGELOG.md](CHANGELOG.md). Keep the
manual-calibration documentation and the applicable bench-test guide aligned
with the code so the next person can reproduce the test safely.

