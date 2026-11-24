# AgriGantry Dashboard

This Flask/Socket.IO dashboard runs a garden simulator by default on the desktop. The Pi can host a hardware telemetry profile or the supervised paired-X bench test. The 3D view uses an optimized FarmBot Genesis v1.8 CAD model and follows the Gabriel-branch motor layout.

- X: dual-motor gantry (`XL`, `XR`) along the bed length
- Y: cross-slide across the gantry
- Z: vertical extrusion and universal tool mount

## Run on Windows

From `Webapp-Pi/webapp`:

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe run.py
```

For a one-click start after setup, open `start_dashboard.py` in VS Code and press **▶ Run Python File**. On a desktop the launcher opens the verified local dashboard in your default browser and prints the same URL. The default is <http://localhost:5000>. It checks the checkout and running Python build before reusing a server. If an earlier build is still running, it looks for a current instance or a free port. Each separately hosted instance has its own simulation state. Use `--no-browser` for a terminal-only launch. Press `Ctrl+C` in the hosting terminal to stop a server you started.

In **Controls**, select **Home all** to start X, Y and Z together, or home each axis individually. Each homing cycle finds both ends and finishes at 50% of travel. The X/Y pad moves across the bed and the Z arrows raise or lower the tool. Each tap travels 10, 25 or 50 mm as selected. **Show 3D** loads the CAD model and its perspective cube. **Stop** discards outstanding travel. **Clear stop** allows a new command.

In **Garden**, drag a seed to one of six spots or select the seed and tap a spot. This saves a plan on the simulator. To preview placement, home all three axes, choose the planned spot and press **Place seed**. Expand **Target coordinates** to adjust X, Y and Z in millimetres from home. X/Y stay inside the selected spot and Z stays within simulated tool travel. The default Z is mid-travel. At arrival the simulator marks the spot **Growing**. An interrupted move returns the seed to **Planned**. Seed pickup, physical sowing and tool changes require later calibration. Garden plans are shared across connected clients and last until the simulator restarts.

In **Garden**, select any **Growing** plant and click **Mark ready to harvest**. Readiness is manual and immediate in this simulation. **Mark harvested** frees the spot. Under **Alerts → Show more**, **Low moisture** and **Harvest ready** target the selected plant. **High temperature** applies to the whole bed, even when it is empty. For a quick demonstration, **Add demo radish** fills an empty spot, and **Preview growing** advances an existing plan. Both work with the simulator stationary. The Garden badge totals open tasks. **View spot** opens the exact plant. Use **Mark watered** to resolve its watering task.

The Controls badge counts XL, XR, Y and Z motor alarms. A motor alarm halts motion and homing. **Clear demo alerts** clears injected motor, temperature and moisture warnings. A ready crop stays ready until harvested. **Clear stop** releases a separately latched stop.

**Bed conditions** shows soil moisture, air temperature, soil pH and relative humidity. **Show sample readings** supplies examples. A check marks a value inside its inclusive demo range. A triangle marks a high or low value or an active plant moisture warning. A dash marks missing, unrated or offline information. The simulator uses 30–60% soil moisture, 18–28 °C, pH 6–7 and 40–70% air humidity as demonstration ranges. A dry spot gets a warning even if the bed-wide moisture sample is in range.

**Tutorial** opens a topic picker for movement and machine view, planting and harvest, watering and conditions, and stops and alarms. Each topic has three short slides. **All topics** returns to the picker. **Close tutorial** or **Esc** returns to the section and garden spot you were using.

## Run on Raspberry Pi or Linux

### Host the Gabriel dashboard over SSH

From Windows PowerShell, run the following from this webapp folder. The Pi username and address below are the ones used on the bench.

```powershell
py -3 tools/host_on_pi.py pi@192.168.1.141
```

Enter the Pi password at the OpenSSH prompt. Open the printed **Phone dashboard** URL from a device that can reach the Pi on the LAN. The Pi serves the UI, assets and controller data. The phone renders the 3D model. Access from cellular or another site requires a route back to this LAN, such as a private VPN.

The default profile runs `run_stm32.py` on the Pi. Add `--profile arduino` for the USB Mega or `--profile simulated` for the garden demo. Each profile serves the same dashboard. The launcher installs dependencies in the Pi user's private virtual environment and checks the running build and profile before printing the URL. The server continues after SSH disconnects. To free the serial port, read the PID from `~/.local/share/agrigantry-gabriel/server.json` on the Pi. Use `ps -p <PID> -o args=` to verify the dashboard runner, then `kill <PID>`.

| Profile | Link and identity | What the phone can verify |
| --- | --- | --- |
| STM32 Test 14 | `/dev/serial0`, 115200 8N1, `PING` → `PONG` plus a valid `STATUS` | New controller reply number, STM32 uptime, X1 encoder counts and the three MCU enable commands. **Check driver SPI** requests one idle register scan, including U4/X1 and U5/X2. The report retains the before/after FAULT bytes, nFAULT samples, SPI status bits and control-register readbacks. |
| STM32 Test 15, `paired-x` | Same COMM wiring and a fresh `PAIR?` response from the firmware | The same idle telemetry, one owner-armed X move, X1 encoder evidence and separate U4/U5 register results. |
| Arduino Mega | `/dev/ttyACM0`, 115200 8N1, `?` firmware identity | Gabriel's `AGRIGANTRY-MANUAL-CAL-v1` reports positions, calibration and motor alarms in `STATUS`. Arturo's `AF434-GANTRY-v1` reports axis positions. Alarm state is unknown when the report omits it. Use `run_arduino.py --port <device>` on the Pi for a different USB port. |

The physical profiles read about once per second and mark a serial timeout or disconnected port in the UI. A browser update never advances the controller sample number. `/api/status` also exposes the last sample time, last SPI scan time and connection state. Leave the serial port to the dashboard during this stage. Test 14 cancels an arm on any other command and interprets a non-newline byte during motion as a stop request.

General jog, homing, planting and demo controls run in the simulator. The `stm32` and `arduino` launch profiles supply physical telemetry. The explicit `paired-x` profile adds the single Test 15 command described below. Absolute travel and axis percentage require a calibrated reference.

### Pi 3 bench result, November 24, 2026

The Gabriel dashboard answered from `192.168.1.141:5000` on the LAN. A Socket.IO client observed two distinct STM32 samples (73 and 74). The JavaScript, stylesheet, seed artwork and 3D model served by the Pi matched the Gabriel checkout byte for byte.

At 15:39:36 EST an explicit **Check driver SPI** request completed over the web connection. U4/X1, U5/X2, U6/Y and U7/Z each returned `SPI=OK`, initial/settled `FAULT=0x00/0x00`, `nFAULT=1/1/1`, `STATUS_BITS=0x00`, `CTRL1=0x0F` and `CTRL2=0x06`. Following the scan the firmware continued replying with X/Y/Z enables `0/0/0` and X1 encoder count `0`. This verifies Pi hosting, live COMM telemetry and independent SPI readbacks including the unloaded X2 driver. Paired-X motor activation and a physical Arduino presentation run are separate tests.

**Phone access passed.** Gabriel opened the Pi-hosted dashboard on his phone, saw the STM32 reply number advance about once per second, and confirmed the displayed driver scan matched the values above. The reply number tracks new UART status samples. The register values and nFAULT levels belong to the most recent driver scan. Seed selection and demo alert triggers are simulator features.

### Run the supervised X-axis test

Test 15 adds one supervised revolution through both X drivers. The bench has a motor and encoder on X1 and an empty X2 motor connector. Leave the Pi COMM leads on their existing pins. If board wiring, ST-LINK target leads or the MOTION_OK jumper needs changing, shut down the board-powered Pi and switch off the bench supply before moving those connections. The existing hardware interlock must permit motion for the physical revolution. Any phone with access to the bench dashboard can arm an idle test. The same phone session must submit the move, and anyone viewing the test can request Stop.

1. With ST-LINK USB on the Pi and its SWD leads on JP1, copy `Firmware/STM32G474/build/Debug/AgriGantry_Rev1_Bringup.elf` from the laptop to the Pi user's home directory as `agrigantry-test15.elf`. In the Pi SSH terminal, run the command below. `Verified OK` confirms the programmed flash matches the image. `Resetting Target` starts the application, with D5 off and all drivers asleep.

   ```bash
   openocd -f interface/stlink.cfg -c "transport select hla_swd" -f target/stm32g4x.cfg -c "program {$HOME/agrigantry-test15.elf} verify reset exit"
   ```
2. In local Windows PowerShell, from `Webapp-Pi/webapp`, start the paired profile. It replaces the Gabriel telemetry server on the Pi and prints the phone URL. The board stays idle when the app starts.

   ```powershell
   py -3 tools/host_on_pi.py pi@192.168.1.141 --profile paired-x
   ```

3. Open the printed URL, select **Controls** and find **X-axis bench test**. The ready state is **Ready for one supervised shaft revolution.** Confirm the STM32 reply number advances and the idle enable values are `0/0/0`. This is a stopping point before any physical activation.
4. For a supervised run with the shaft clear, tap **Arm X**, then **Run one revolution** within 30 seconds. Keep that phone on the controls and watch X1. D5 lights during the pulse train and goes dark when the outputs turn off. **Stop X** interrupts a running test. Losing the operator connection or leaving the controls also requests a stop. If the Pi or its serial link fails, the STM32 stops when its UART heartbeat expires.
5. Read the result on the phone. Look for 3,200 pulses, about 4,000 signed X1 counts, one index and both driver reports. The direction in the earlier X1-only run gave −3,999. Expand **Driver readbacks** for register details or for an interrupted run. After completion the reply counter resumes and X/Y/Z should read `0/0/0`. The app displays partial travel on a stop. Reopening the page retrieves the last result. Neither reloading nor tapping twice repeats the command. The original `stm32` launch profile restores observation-only operation.

**Preparation on November 24, 2026.** The ARM build, 12 native firmware cases and 22 dashboard tests passed. The decoder also accepted the UART result format emitted by the compiled C tests.

**Phone-commanded revolution passed on hardware the same day.** Gabriel saw X1 rotate. The dashboard showed `DONE`, 3,200 pulses, −3,998 X1 encoder counts and one index crossing. The Pi retained a 19,856 ms motion window. Both X drivers verified their settings and returned 34 diagnostic samples each. X2 participated with its motor socket empty. Afterward, the controller reported X/Y/Z enable commands `0/0/0`, and Gabriel observed D5 off. [Full hardware result and driver readbacks](../../Firmware/STM32G474/README.md#test-15-hardware-result)

### Run directly on the Pi

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/python run.py
```

Open the `On the WiFi` address printed at startup from a phone or laptop on
the same network.

## Important files

- `run.py`: simulated backend and server entry point
- `config.py`: dimensions, motors, simulation rate, and server settings
- `static/js/scene3d.js`: Three.js rendering and X/Y/Z model motion
- `static/models/farmbot_model.glb`: optimized browser model
- `tools/prepare_farmbot_model.py`: reproducible CAD cleanup/optimization

The original Onshape export is intentionally not committed. To rebuild the
web model, install the model-only tools outside the runtime environment:

```powershell
py -m venv model-tools
.\model-tools\Scripts\python.exe -m pip install trimesh fast-simplification numpy
.\model-tools\Scripts\python.exe tools\prepare_farmbot_model.py `
  "C:\path\to\FarmBot Genesis v1.8 Beta.glb" `
  static\models\farmbot_model.glb
```

The preparation script removes electronics, wiring, plumbing, cameras, and
tiny fasteners. It also makes these deliberate visual simplifications:

- All drag chains are excluded. A real drag chain bends between fixed and
  moving endpoints; the CAD chain is rigid and cannot follow the axes
  correctly without a separate procedural animation.
- The raised bed, soil, and X tracks are displayed at 65% of the official
  model's X length for evaluation. Simulation coordinates remain 0..2800 mm,
  mapped proportionally onto the shorter visual tracks.
- The two X-axis belt runs that rise to the moving gantry motors are excluded.
  Two simple flat belts remain along the tracks. The Y-axis belt is unchanged.
- The central wooden post inside the raised-bed soil area is excluded because
  browser mesh simplification distorted it into an obstructive triangular
  wedge. The bed walls and all exterior structure remain.

Tools remain named groups, and the model uses the nested motion hierarchy
`Gantry_X -> Y_Carriage -> Z_Extension`.

## Optional 3D view

The 3D view is off by default. Its Three.js module, WebGL context, and GLB are
loaded only after **Show 3D** is selected. A one-sided grass plane below the model keeps the bottom camera view usable. **Hide 3D** returns to the bed overview. Switching to Garden or Tutorial pauses rendering while the simulator continues running.

When 3D is on, the navigation cube in the upper-right selects top, bottom,
front, back, left, and right camera views. Each of its eight corners selects
the matching diagonal/isometric view. Click a face or corner to snap the
camera; the cube rotates to match the chosen view and is completely hidden
while 3D is off.

## Raspberry Pi 3 note

The Flask server and simulation are light. The browser displaying the page is
responsible for rendering the 3D scene:

- If the Pi hosts the server and a laptop/phone opens the WiFi URL, the client
  device does nearly all graphical work.
- If Chromium runs directly on the Pi's HDMI display, the Pi must both host and
  render. Expect lower frame rates; use a modest display resolution and avoid
  other heavy applications.

The scene renders only when something changes, caps display pixel density,
uses no shadows, and keeps the GLB small enough for local-network use.
