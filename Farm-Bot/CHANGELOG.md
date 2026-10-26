# Changelog

This file records main changes that affect the Farm-Bot system design or operation.

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
- Added the VS Code PlatformIO IntelliSense configuration so editor diagnostics obtain the Arduino framework include paths; this resolves false `Arduino.h` missing-header errors when the `Farm-Bot` PlatformIO project folder is opened.
- Renamed the active ICL-based motor library to `Motors_ICL` and reserved `Motors_Enc` for the 17HS19-2004-ME1K magnetic-encoder motor. The new library is intentionally documentation-only until the motor passes its standalone bench test.
- Added the isolated `megaatmega2560-encoder-test` firmware target for the 17HS19-2004-ME1K and TB6600 bench test. It excludes the FarmBot main program, reads the encoder's A+/B+/Z+ signals on Mega interrupts, and writes CSV-compatible movement results to Serial for capture in `readings_enc.csv`.
- Updated the encoder bench-test pin map to A+/B+/Z+ on Mega D2/D3/D20 and the TB6600 reference setting to 400 pulses per revolution. All three encoder inputs now use Mega hardware interrupts; Z is counted on its rising edge rather than through polling.
