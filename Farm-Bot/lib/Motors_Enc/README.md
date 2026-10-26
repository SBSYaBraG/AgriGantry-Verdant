# Motors_Enc: 17HS19-2004-ME1K Bench Test

This library is a standalone test harness for the new 17HS19-2004-ME1K NEMA 17 stepper motor and its external magnetic encoder. It does not control the FarmBot gantry, use homing switches, or perform closed-loop correction.

## Why this test exists

The existing ICL motors contain their encoder and controller together, so the FarmBot controller cannot read or experiment with their feedback. The 17HS19-2004-ME1K exposes its magnetic encoder as separate A, B, and Z differential signal pairs. This bench test uses an AM26LV32E differential receiver to convert them to 3.3 V logic for the Arduino Mega.

## Goals

1. Confirm the TB6600 drives both motor coils correctly at 1.5 A.
2. Confirm command pulses move the shaft in both directions.
3. Confirm the encoder A and B signals change and produce a sensible quadrature count.
4. Confirm the Z index channel is visible once per revolution.
5. Compare commanded pulses against encoder counts at full step, then repeat at a known microstep setting.
6. Check low- and medium-speed ramping without a mechanical load.

This test does **not** claim final gantry accuracy, load torque, closed-loop correction, or noise immunity. It does provide a controlled comparison of the receiver input waveform and decoded encoder counts with no termination and with several shunt-termination values.

## Required wiring

### Motor coils to TB6600

| 17HS19-2004-ME1K wire | TB6600 terminal |
| --- | --- |
| Red | A+ |
| Black | A- |
| Yellow | B+ |
| Blue | B- |

### Mega command pins to TB6600

| Mega | TB6600 terminal | Notes |
| --- | --- | --- |
| D4 | PUL+ | Step output |
| D5 | DIR+ | Direction output |
| GND | PUL-, DIR-, ENA- | Common-cathode input return |
| D6 | ENA+ | Active-low offline control: LOW only during a commanded move; HIGH at idle |

Set the TB6600 current to approximately **1.0 A RMS / 1.2 A peak** and the microstep row to **1600 pulses per revolution**. The program's pulse-to-angle calculation assumes that setting. Update `TB6600_PULSES_PER_REV` in `EncConfig.h` before testing another microstep setting.

### Encoder, AM26LV32E receiver, and Mega

Power and common reference:

| Connection | Destination |
| --- | --- |
| Encoder Red (VCC) | Mega 5V |
| Encoder Black (GND) | Mega GND |
| AM26 pin 16 (VCC) | Mega 3.3V |
| AM26 pin 8 (GND) | Mega GND |
| AM26 pin 4 (G) | Mega 3.3V |
| AM26 pin 12 (/G) | Mega GND |
| 0.1 uF ceramic | Directly between AM26 pins 16 and 8 |

Signal connections:

| Encoder wire | Signal | AM26LV32E | Mega |
| --- | --- | --- |
| Brown | A+ | Pin 2 (1A) | Pin 3 (1Y) to D2 |
| Orange | A- | Pin 1 (1B) | - |
| Blue | B+ | Pin 6 (2A) | Pin 5 (2Y) to D3 |
| Green | B- | Pin 7 (2B) | - |
| Yellow | Z+ | Pin 10 (3A) | Pin 11 (3Y) to D18 |
| White | Z- | Pin 9 (3B) | - |

D2, D3, and D18 all use Mega external interrupts, so A/B quadrature edges and Z index rising edges are counted without polling. D18 is also TX1; this firmware uses only `Serial`, so it must remain the only serial interface in use during this test.

Start with no termination or filter capacitors. For later tests, install exactly one resistor directly across each input pair on the receiver breakout: A+ to A-, B+ to B-, and Z+ to Z-. Test 120 ohm, then 100 ohm, then 82 ohm. These are shunt resistors; never put one in series with a signal wire.

## Test sequence

1. Keep the shaft clear and test with no mechanical load.
2. Set the TB6600 to approximately 1.0 A RMS / 1.2 A peak and 1600 pulses/revolution.
3. Upload the `megaatmega2560-encoder-test` environment and open Serial Monitor at 115200 baud.
4. With power off, confirm the AM26 wiring and its 0.1 uF decoupling capacitor. Start with no termination resistors.
5. Send `STATUS`, then turn the shaft by hand and send `STATUS` again. A/B and the encoder count must change.
6. For the first scope capture, probe A+ and A- at AM26 pins 2 and 1. Connect both probe ground leads only to Arduino GND, use Math `CH1 - CH2`, and never connect a scope ground lead to either differential signal.
7. Send `ZERO`, then `F 1600`. This generates 1600 STEP pulses: one mechanical revolution at the configured 1600 pulses/revolution. It runs at the conservative 100 pulses/s setting, giving the scope a repetitive waveform for roughly 16 seconds including firmware overhead.
8. The post-move encoder magnitude should be near 4000 counts. Its sign depends on motor/encoder orientation. Record the Z/index result, but do not assume one Z pulse per mechanical revolution until the Z waveform is verified.
9. Repeat the capture for B+ / B- at AM26 pins 6 / 7. Then probe AM26 outputs A (pin 3) and B (pin 5) to inspect 3.3 V logic levels and quadrature.
10. Repeat the same capture sequence after adding 120 ohm, 100 ohm, and 82 ohm shunt termination to every A, B, and Z pair. The serial monitor preserves each completed `ENC_LOG` row for the count summary; export scope CSV files separately for waveform analysis.

## Recording and comparing results

The Arduino does not create a waveform CSV. It reports one `ENC_LOG` count summary after each completed move. PlatformIO is configured with the `log2file` monitor filter, so each serial-monitor session is automatically saved under the project `logs` directory.

Before each capture, label it using an alphanumeric identifier with `_` or `-`, then run the move. For example:

```text
TAG NONE_A
ZERO
F 800
```

After closing the Serial Monitor, create a clean Arduino summary CSV without overwriting historical `readings_enc.csv`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\export_encoder_readings.ps1
```

This creates `readings_enc_YYYYMMDD_HHMMSS.csv` with one row per completed move. It records the local test timestamp, its source, capture label, commanded pulses, duration, encoder delta, expected count, error, and index counts. Export the Tektronix waveform CSV separately and give it the same date/time and label, for example `20260916_142355_NONE_A_scope.csv`. Use the Arduino CSV to compare reliable count/direction results, and the scope CSV for voltage, timing, ringing, and settling measurements.

For every commanded move, each Z rising edge is also printed after `ENC_LOG` as:

```text
Z_EVENT,sequence,time_us,encoder_count
```

`time_us` is the Arduino `micros()` value at the Z edge and `encoder_count` is the nearest completed A/B quadrature count at that instant. The events are captured only while commanded STEP pulses are being generated, stored in a 64-entry ISR buffer, and emitted after motion, so serial printing and motor release cannot create or hide edges. A `Z_EVENT_OVERFLOW,<count>` row means more than 64 Z edges occurred during one move and that run must be repeated with a shorter move.

## Serial commands

| Command | Action |
| --- | --- |
| `HELP` | Print the command list |
| `STATUS` | Print encoder inputs, count, and index count |
| `ZERO` | Set encoder and index counts to zero |
| `TAG <label>` | Set the capture label included in subsequent `ENC_LOG` rows |
| `F <pulses>` | Forward constant-speed move at 100 pulses/s |
| `R <pulses>` | Reverse constant-speed move at 100 pulses/s |
| `RF <pulses> <pps>` | Forward move with acceleration/deceleration |
| `RR <pulses> <pps>` | Reverse move with acceleration/deceleration |

Each completed move prints one CSV-compatible `ENC_LOG` row. The expected encoder count is based on 4000 encoder counts/revolution and the configured TB6600 pulses/revolution. A positive or negative encoder delta is valid; its sign depends on coil and encoder orientation.
