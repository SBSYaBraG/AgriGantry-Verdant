"""Package the firmware as one self-contained Arduino IDE sketch.

Run: python Farm-Bot/tools/export_sketch.py
The resulting Arduino/AgriGantry/AgriGantry.ino needs no project headers or
extra library files. Normal PlatformIO builds do not overwrite this sketch.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
LIB = ROOT / "Farm-Bot/lib/Motors_X"
HEADERS = ("Config.h", "PositionManager.h", "MotorControl.h",
           "ALM_Monitor.h", "SystemOperations.h", "CommandProcessor.h")
SOURCES = (LIB / "PositionManager.cpp", LIB / "ALM_Monitor.cpp",
           LIB / "MotorControl.cpp", LIB / "SystemOperation.cpp",
           LIB / "CommandProcessor.cpp", ROOT / "Farm-Bot/src/main.cpp")


def source_text(path):
    text = path.read_text(encoding="utf-8")
    return re.sub(r'^\s*#include\s+"(?:' + '|'.join(re.escape(h) for h in HEADERS)
                  + r')"[^\n]*\n', '', text, flags=re.MULTILINE).strip()


def main():
    parts = ["""/*
  AgriGantry - COMPLETE SINGLE-FILE ARDUINO FIRMWARE

  Upload THIS file to the Arduino, just like the AF-434 sketch:
    1. Open AgriGantry.ino in Arduino IDE.
    2. Select Arduino Mega or Mega 2560 and its port.
    3. Click Upload.

  Then run Pi/gantry_keyboard.py on your COMPUTER for keyboard control.
  W/S = X, A/D = Y, Q/E = Z; release to slow down; O = emergency stop.
  Hardware: M1=X-left, M2=X-right, M3=Y, M4=Z. No limit switches.

  Everything the Arduino needs from this project is in this one file.
  Pin assignments and optional travel settings are directly below.
*/
#include <Arduino.h>
"""]
    for header in HEADERS:
        parts.append(f"// ---------------- {header} ----------------\n{source_text(LIB / header)}\n")
    # Explicit declarations stop Arduino's automatic prototype generator from
    # placing Axis/Motor/MotorALM arguments before their types are declared.
    declarations = []
    for source in SOURCES:
        declarations += [match.group(1) + ";" for match in re.finditer(
            r'^(static\s+[^;{\n]+\([^;{\n]*\))\s*\{', source_text(source), re.MULTILINE)]
    parts.append("// Internal declarations\n" + "\n".join(declarations) + "\nvoid setup();\nvoid loop();\n")
    for source in SOURCES:
        parts.append(f"// ---------------- {source.name} ----------------\n{source_text(source)}\n")
    sketch = ROOT / "Arduino/AgriGantry/AgriGantry.ino"
    sketch.parent.mkdir(parents=True, exist_ok=True)
    sketch.write_text("\n".join(parts), encoding="utf-8", newline="\n")
    print(f"Single-file sketch: {sketch} ({len(sketch.read_text().splitlines())} lines)")


if __name__ == "__main__":
    main()
