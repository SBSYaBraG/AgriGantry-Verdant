# AgriGantry firmware changelog

This file records main changes that affect the AgriGantry system design or operation.

## 2026-11-08 — 21:11 EST

- Renamed the PlatformIO project directory from `Farm-Bot/` to `Firmware/` and the Pi/dashboard source directory from `Pi/` to `Webapp-Pi/`. The outer Git repository directory remains unchanged until the repository itself is renamed separately. Updated PlatformIO IntelliSense/debug paths, documentation links, build instructions, and future Pi integration paths to match.
- Renamed the public project identity to **AgriGantry** in the repository and firmware documentation, normal-firmware serial banners, dashboard/server labels, and the manual-calibration firmware identifier (`AGRIGANTRY-MANUAL-CAL-v1`). References to the official FarmBot CAD model and the preserved legacy FarmBot controller remain explicitly labelled as reference/legacy material.
- Moved the existing encoder CSV readings into `data/encoder/arduino_exports/` and changed `scripts/export_encoder_readings.ps1` to write future summaries there. PlatformIO's live serial-monitor captures remain in `Firmware/logs/`, where the exporter continues to find the newest capture. Added `data/README.md` and reserved structured locations for scope exports and future manual-calibration evidence.
- Added `Webapp-Pi/README.md` to distinguish the simulated dashboard, the Pi integration contract, and the incompatible legacy controller from the current physical-manual-calibration workflow. Kept the existing ignored `Pi/.venv` local and unmoved because Python virtual environments contain absolute paths and must be recreated rather than renamed.

## 2026-11-08 — 14:55 EST

- Reduced the manual-calibration firmware's normal serial-monitor noise. Boot now emits only the firmware identifier and one `READY` line with all drivers disabled; the complete command reference is still available only when `HELP` is explicitly requested. Calibration prompts and percentage-format errors were condensed while retaining endpoint confirmations, calibration totals, move results, alarms, and E-stop messages needed during operation.
- Documented in the manual motion/calibration source that `travel_steps` and reported positions are commanded-pulse estimates in the present iCL setup. Added encoder-integration TODOs requiring future logs to separately record encoder deltas and command-versus-feedback error rather than treating a matching commanded count as proof of physical position or no drift.
- Added `Webapp-Pi/MANUAL_CALIBRATION_INTEGRATION.md` as a non-executable implementation contract for the future Pi service and web-app tab. It defines Mega/Pi/browser ownership, current serial commands, supervised 0-100% and safe 10-90% test profiles, arm/disarm rules, serial timeouts, JSONL/CSV plus Markdown log requirements, and the later encoder/DRV8461 extension. It intentionally starts no scheduler, opens no serial port, and changes no web-app runtime behavior.

## 2026-10-27

- Added a separate `megaatmega2560-manual-calibration` PlatformIO target for the currently installed iCL42-08 system. It builds only `manual_calibration_main.cpp`, so it cannot accidentally link the normal AgriGantry program or the encoder bench-test firmware. Startup explicitly leaves every PUL line LOW and every driver disabled; it performs no automatic motion or physical homing.
- Disabled `AUTO_HOME_ON_STARTUP` in the normal `Motors_ICL` configuration as an additional protection against unexpected movement when the Mega is connected directly to a computer.
- Added the `Motors_Manual` calibration/motion/safety/protocol library using Arturo's four-driver AF-434 pin map: dual X drivers, one Y driver, and one Z driver. It maintains one active axis at a time and counts every commanded pulse, including the deceleration pulses after a jog key is released or a controlled stop is requested.
- Added a manual, no-limit-switch endpoint workflow for each axis: jog carefully to either physical endpoint, wait two seconds and explicitly confirm it, reverse direction to capture software 0%, then jog to and confirm the opposite endpoint. The confirmed pulse count becomes that axis's 0-100% travel range for the current power session.
- Kept X, Y, and Z calibration records independent. Completing or repeating calibration on one axis does not erase the other calibrated axes. After an axis is calibrated, free jogging is locked and only bounded percentage moves are accepted.
- Added `HOME <axis>` as a safe return to the calibrated 50% midpoint rather than a physical hardstop search. Added guarded `HOME ALL`, exposed as `G` in the keyboard controller, which returns X, then Y, then Z to their 50% midpoints only after all three have a trusted current-session calibration. A controlled stop aborts the sequence instead of continuing to the next axis.
- Added `scripts/manual_keyboard_controller.py` and its Python requirements for computer-keyboard operation over serial: F1/F2/F3 select X/Y/Z; held A/D, Left/Right, and W/S jog X/Y/Z during calibration; Enter confirms endpoints; 1-5 request 10/25/50/75/90%; Space requests a ramped stop; and O or Esc performs an immediate latched emergency stop.
- Added driver ALM monitoring, serial jog-heartbeat loss protection, and explicit fault clearing. Any asserted ALM input, heartbeat loss, or emergency-stop command immediately disables the drivers; `CLEAR` is rejected until every ALM input is normal. ALM is deliberately treated only as a coarse iCL fault signal, not as raw encoder feedback.
- Added conservative acceleration/deceleration ramps and axis-specific cruise speeds: calibration X/Y/Z = 2500/2000/1500 pulses/s and normal percentage/midpoint moves X/Y/Z = 3000/2500/1800 pulses/s. Confirmed travel lengths and positions intentionally remain RAM-only until a later encoder/stall-reference design can safely re-establish physical position after power loss.

## 2026-09-28

- Updated the encoder bench-test driver configuration to the new physical TB6600 setting of **1600 STEP pulses per mechanical revolution**. The expected-count calculation now correctly treats `F 1600` as one mechanical revolution and expects about 4000 encoder counts. The 200-pulses/revolution result below remains the record of the prior physical DIP-switch configuration.
- Corrected the encoder bench-test TB6600 configuration to the bench-verified **200 STEP pulses per mechanical revolution**. The previous 400-pulses/revolution software setting caused expected-count errors even though the motor and encoder were behaving correctly: 200 commanded pulses measured approximately 4000 encoder counts and 400 pulses approximately 8000. `TB6600_PULSES_PER_REV` now drives both the calculations and the startup display, reducing the chance of a future hard-coded mismatch.
- Added per-edge Z diagnostic logging to the isolated encoder bench firmware. Each Z rising edge during a commanded move now records the Arduino `micros()` timestamp and instantaneous A/B quadrature count in an ISR-safe 64-event buffer; `Z_EVENT,sequence,time_us,encoder_count` rows are emitted after the move. Buffer overflow is explicitly reported rather than silently losing data.
- Removed the unverified assumption that Z must generate one pulse per mechanical revolution. The new trace distinguishes a multi-pulse Z source from repeated receiver/input transitions near one physical position.

## 2026-09-16

- Updated the isolated `megaatmega2560-encoder-test` bench firmware to match the differential-receiver wiring: TB6600 PUL/DIR/ENA on Mega D4/D5/D6 and AM26LV32E A/B/Z receiver outputs on D2/D3/D18.
- Matched the proven `Motors_ICL` TB6600 enable behavior: D6 drives ENA+ LOW only during a commanded encoder-test move, then HIGH at idle; ENA- shares Arduino GND with PUL- and DIR-.
- Bench verified the active-low enable behavior: with D6 HIGH the shaft is free, and `F`/`R` commands pull D6 LOW, energize the motor after a 200 ms settling delay, execute the STEP pulses, then return D6 HIGH. The initial no-motion failure came from treating ENA as an active-high enable instead of reusing the existing ICL polarity.
- Added labeled, file-backed measurement summaries: `TAG <label>` is included in every completed `ENC_LOG` row, PlatformIO monitor output is logged automatically, and `scripts/export_encoder_readings.ps1` produces a clean date-stamped encoder summary from the latest serial session. This preserves separate Arduino count/direction and Tektronix waveform data sources.
- Added date/time traceability for bench captures: PlatformIO now timestamps every serial line, and the CSV exporter writes per-move local timestamps plus a timestamp-source field to date-stamped `readings_enc_YYYYMMDD_HHMMSS.csv` outputs.
- Retained the existing `F 800` two-revolution command and post-move CSV summary. It is suitable for validating the encoder count and direction after a scope capture; oscilloscope CSV remains the authoritative waveform measurement.

## 2026-08-26

- Renamed the PlatformIO library directory from `Motors_X` to `Motors`. The library controls all axes, so the new name reflects its shared role.
- Changed the active hardware configuration from five motors to four: two X motors, one Y motor, and one Z motor. The former second Z motor's ALM pin, enum value, and ALM mapping remain commented out for a future dual-Z restoration.
- Added the VS Code PlatformIO IntelliSense configuration so editor diagnostics obtain the Arduino framework include paths; this resolves false `Arduino.h` missing-header errors when the `Firmware` PlatformIO project folder is opened.
- Renamed the active ICL-based motor library to `Motors_ICL` and reserved `Motors_Enc` for the 17HS19-2004-ME1K magnetic-encoder motor. The new library is intentionally documentation-only until the motor passes its standalone bench test.
- Added the isolated `megaatmega2560-encoder-test` firmware target for the 17HS19-2004-ME1K and TB6600 bench test. It excludes the AgriGantry main program, reads the encoder's A+/B+/Z+ signals on Mega interrupts, and writes CSV-compatible movement results to Serial for capture in `readings_enc.csv`.
- Updated the encoder bench-test pin map to A+/B+/Z+ on Mega D2/D3/D20 and the TB6600 reference setting to 400 pulses per revolution. All three encoder inputs now use Mega hardware interrupts; Z is counted on its rising edge rather than through polling.
