# AgriGantry Dashboard

This Flask/Socket.IO dashboard runs against `SimulatedHardware`; it does not
command the physical AgriGantry. The 3D view uses an optimized FarmBot Genesis
v1.8 CAD model and follows the Gabriel-branch motor layout:

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

Open <http://localhost:5000>, select **HOME ALL AXES**, then use the jog or
percentage controls. The 3D view starts off; select **3D VIEW: OFF** to load
and enable it. Press `Ctrl+C` in the terminal to stop the server.

The simulation currently runs X at 120 mm/s and Y/Z at 80 mm/s. These are
demonstration speeds, not measured physical-machine values. An active motor
ALM immediately stops its axis and blocks further movement or homing on that
axis until **Clear all alarms** is selected; the other axes remain available.

## Run on Raspberry Pi or Linux

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
loaded only after **3D VIEW: OFF** is selected. Turning the view off again
stops its render loop; the controls, simulation, numeric positions, and
minimap continue operating normally.

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
