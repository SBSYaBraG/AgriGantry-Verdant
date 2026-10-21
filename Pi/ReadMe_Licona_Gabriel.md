# AgriGantry controller using AF-434 wiring

The Arduino Mega drives four motors: M1 = X-left, M2 = X-right, M3 = Y,
and M4 = Z. Pin assignments and direction polarities come from the working
AF-434 sketch. There are no limit switches and no automatic homing.

Read the [PDF program guide](../docs/AgriGantry_Program_Guide.pdf) for a detailed
plain-English explanation, wiring table, commands, and verification results.
A [plain-text copy](../docs/AgriGantry_Program_Guide.txt) is also included.

Upload the complete single-file sketch
**[Arduino/AgriGantry/AgriGantry.ino](../Arduino/AgriGantry/AgriGantry.ino)**
through Arduino IDE, just like AF-434. Select **Arduino Mega or Mega 2560**,
select its port, and click **Upload**. The file contains all Arduino firmware.
Close Serial Monitor, then run these commands from the repository root:

```powershell
python -m pip install -r Pi/requirements.txt
python Pi/gantry_keyboard.py --port COM9
```

Replace `COM9` with the Mega's port, or omit `--port` for automatic detection.
Use `--list-ports` to list the available ports. Run one controller at a time.

Startup explicitly resets the Mega through DTR, clears old serial transfers,
and waits for the board before checking its firmware ID. This recovers the
COM9 write timeout seen when reopening the connection alone did not reset it.
Motor alarms reported during startup are displayed and keep keyboard motion
blocked until the cause is resolved and C successfully clears the stop.
If a write still fails, the error identifies the command and port; close Serial
Monitor, unplug/reconnect the USB cable, and restart the Python controller.

Hold a movement key to jog, then release it to slow that axis to a stop:

| Key | Action |
| --- | --- |
| **W / S** | X forward / backward; both X motors together |
| **A / D** | Y left / right |
| **Q / E** | Z up / down |
| **Space** | Smoothly stop all axes |
| **O** | Immediate, latched emergency stop on all motors |
| **C** | Clear the stop after resolving any motor alarms |
| **+ / -** | Raise / lower jog speed |
| **P** | Show positions, velocities, travel, and stop state |
| **0** | Record the manual origin, only while stopped |
| **Esc** | Smoothly stop and exit |

X, Y, and Z can jog together. Opposite keys on an axis use the most recent
press; releasing it returns control to the other held key. Keys are captured
globally, so stop the keyboard controller before typing elsewhere.

The motion engine uses AF-434's 20 kHz Timer1 pulse generator and 1 kHz
velocity control. Normal starts, releases, and reversals ramp at
`MOTION_ACCELERATION` (3000 steps/s²). Default jog speed is 1000 steps/s,
adjustable from 100 to 8000. At the default speed, a normal release takes about
0.33 seconds and 167 steps to stop. Finite step commands also brake before
reaching their targets. A missing heartbeat for 500 ms starts a smooth jog stop.
O and motor alarms stop pulses immediately and retain holding torque.

**Press O to stop every motor without Enter.** The Python controller's global
keyboard listener stays active while waiting for a command or move response.
It accepts either case and reserves O for stopping. A desktop session with
keyboard access and the packages in `requirements.txt` are required. If the
listener cannot start, the controller reports the problem and exits.

The stop latches in the firmware until `CLEAR` or `S0`. It stops step pulses
and retains motor holding torque. The stop also invalidates manual position
references. An active driver alarm prevents clearing the latch.
After clearing, release and press movement keys again; old held keys do not
restart movement automatically.

For typed step and percentage commands, use the original command interface
instead of the keyboard controller:

```powershell
python Pi/Pi_FarmBotController_Class.py --port COM9
```

| Command | Result |
| --- | --- |
| `X1000`, `Y-500`, `Z100` | Relative steps with smooth acceleration and braking |
| `PX25`, `PY50`, `PZ75` | Percentage of configured travel after manual reference |
| `0` | Record all current positions as the manual origin; no motion |
| `0X`, `0Y`, `0Z` | Record one axis's origin |
| `O` | Stop all motors immediately; no Enter in the Python controller |
| `CLEAR`, `S0` | Clear the stop after resolving any alarms |
| `STATUS`, `R` | Report positions, velocities, references, and configured travel |
| `ALM` | Show all four driver alarms |
| `S`, `STOP`, `!` | Other supported stop commands |
| `HELP` | Firmware help |
| `H`, `HX`, `HY`, `HZ`, `HALL` | Explain that homing is unavailable; no motion |

Set measured `X_TRAVEL_STEPS`, `Y_TRAVEL_STEPS`, and `Z_TRAVEL_STEPS` in
the settings near the top of `Arduino/AgriGantry/AgriGantry.ino`, upload, and manually set the origin
with `0` before using percentages. Relative moves work without that reference;
software travel bounds exist only when a reference and measured travel are set.
Position is counted from issued pulses, rather than read from the motor encoders.

The keyboard stop requires a working controller, USB connection, and Arduino.
In a Serial Monitor, pressing O may still require Send/Enter because of the
monitor's text-entry behavior. The Python listener sends it on the keypress.

Verification commands, from the repository root:

```powershell
python Farm-Bot/tools/build_mega.py
python Farm-Bot/test/run_native.py
python Farm-Bot/test/run_native.py --standalone
python -m unittest discover -s Pi -p 'test_*.py'
python Farm-Bot/tools/make_guide.py
```

The AVR builder requires an installed Arduino AVR board package; it generates
a HEX in `.build/mega2560` without uploading. Native firmware tests use MSVC
and simulated pins/serial. Python tests simulate the keyboard and connection.
