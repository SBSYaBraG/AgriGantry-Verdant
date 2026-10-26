# Manual calibration firmware

This is an intentionally separate Mega 2560 firmware for manually measuring
the travel of the currently connected ICL drivers. It uses Arturo's active
four-driver wiring, not `Motors_ICL`'s older pin assignments:

| Axis / motor | PUL | DIR | ENA | ALM |
|---|---:|---:|---:|---:|
| X-left | 9 | 8 | 22 | 26 |
| X-right | 7 | 6 | 23 | 27 |
| Y | 5 | 4 | 24 | 28 |
| Z | 3 | 2 | 25 | 29 |

## Safety model

- Build only `megaatmega2560-manual-calibration`. It excludes `main.cpp` and
  `encoder_test_main.cpp`.
- Startup leaves every driver disabled and all PUL pins low.
- A HIGH ALM input, `O`/`Esc`, or lost jog heartbeat latches a stop and disables
  all drivers.
- While calibrating, one axis may jog only while its keyboard key is held.
- After calibration, jogging is locked. Use percentage commands or `HOME`.
- `HOME` moves to the measured software 50% midpoint; it never drives into a
  physical endpoint and is not automatic homing.
- `HOME ALL` returns X, then Y, then Z to their 50% midpoints. It refuses to
  start unless all three axes were calibrated in this same power session.

## Calibration flow

1. Select an axis, start calibration, and jog it carefully to either endpoint.
2. Release the key. After two seconds, press Enter to confirm that endpoint.
3. Press the opposite jog direction. Before its first step the controller sets
   that confirmed endpoint to 0% and counts pulses toward the other end.
4. At the far endpoint, release, wait two seconds, and press Enter again.
5. The measured pulse count is the 0–100% travel range for this power session.

Travel ranges and positions are deliberately RAM-only. A power cycle requires a
new manual reference; later encoder/stall homing can replace that requirement.

## Build and run

From `Farm-Bot`:

```powershell
pio run -e megaatmega2560-manual-calibration -t upload
python -m pip install -r scripts/requirements-manual.txt
python scripts/manual_keyboard_controller.py --port COM9
```

Keyboard controls: `F1`/`F2`/`F3` select X/Y/Z, `C` begins calibration,
`A`/`D` jog X, Left/Right jog Y, `W`/`S` jog Z, Enter confirms an endpoint,
and `1`–`5` request 10/25/50/75/90% on the selected calibrated axis.

`H` returns the selected calibrated axis to 50%. `G` sends `HOME ALL`.

## Future work

The ICL ALM line is used only as a fault stop. Future DRV8461 work should add
proper differential receivers for raw encoder A/B/Z feedback and a tested stall
mismatch detector before enabling automatic hardstop homing.
