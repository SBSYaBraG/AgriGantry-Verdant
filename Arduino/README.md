# Upload this sketch to the Arduino

Open **[AgriGantry/AgriGantry.ino](AgriGantry/AgriGantry.ino)** in Arduino IDE.
This one file contains the complete gantry firmware, including the AF-434
motor pins, paired X motors, smooth acceleration/deceleration, alarms, and O stop.

1. Select **Tools > Board > Arduino AVR Boards > Arduino Mega or Mega 2560**.
2. Select **ATmega2560** if a Processor menu is shown.
3. Select the Mega's port under **Tools > Port**.
4. Click **Upload**.

Close Serial Monitor, then run the keyboard controller on your computer:

```powershell
python -m pip install -r Pi/requirements.txt
python Pi/gantry_keyboard.py --port COM9
```

Run these commands from the repository root and replace `COM9` with the
Mega's port. Hold W/S for X, A/D for Y, and Q/E for Z; release to slow to a
stop. Space smoothly stops all axes, O emergency-stops, and C clears the latch.

The `.ino` is self-contained: no project headers or additional Arduino libraries
need copying. Pin and travel settings are near its top. Python runs on the
computer and sends keyboard commands over USB.

For development, the same firmware is also maintained in the existing
`Farm-Bot` PlatformIO project. To regenerate this sketch from those source
files, run `python Farm-Bot/tools/export_sketch.py`. That command replaces the
`.ino` with the current project sources, so preserve any edits made directly
to the sketch before regenerating it.
