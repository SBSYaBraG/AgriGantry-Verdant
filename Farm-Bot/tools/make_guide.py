"""Generate the program guide as a PDF and plain text, using only Python's stdlib."""
from pathlib import Path
import textwrap

ROOT = Path(__file__).resolve().parents[2]
DOC = [
    ("title", "AgriGantry: how it works"),
    ("subtitle", "AF-434 wiring + AgriGantry commands | Arduino Mega 2560"),
    ("h", "The quick explanation"),
    ("p", "Your computer controls the gantry with hold-to-move keys or typed step commands. The Arduino turns those requests into electrical pulses on the same pins as the working AF-434 program. Motors 1 and 2 move both sides of X together; motor 3 moves Y; motor 4 moves Z. Hold W/S, A/D, or Q/E to move, then release the key to slow smoothly to a stop. All three axes can jog together. Press O to stop every motor immediately without Enter; that stop stays latched until C in keyboard mode or CLEAR in command mode. There are no limit switches, so startup produces no automatic motion or homing."),
    ("h", "The wiring it actually uses"),
    ("code", "Motor / axis      PUL   DIR   ENA   ALM   Positive DIR level\nM1 / X-left        9     8    22    26   HIGH\nM2 / X-right       7     6    23    27   HIGH\nM3 / Y             5     4    24    28   HIGH\nM4 / Z             3     2    25    29   LOW"),
    ("p", "These are Mega digital pin numbers, copied from AF-434/gantry_control/gantry_control.ino at commit 873e2b6. The executable reference sets both X motors' positive-is-clockwise settings to true, even though parts of its README describe opposite rotation. This version follows that executable configuration. Positive X means AF-434's forward direction, positive Y means right, and positive Z means up. A negative step count reverses the relevant direction pins."),
    ("p", "ENA LOW enables a driver and retains holding torque; ENA HIGH frees it. ALM HIGH means a fault and ALM LOW means normal. Each motor has its own PUL, DIR, ENA, and ALM pins. The old five-motor assumption and the old limit-switch pin assignments are gone. The AF-434 reference repository was used for reading only."),
    ("h", "Where the main parts live"),
    ("code", "Arduino/AgriGantry/AgriGantry.ino  Complete Arduino firmware\n  Settings near the top          Pins, directions, travel\n  Motor control section          Timer1 pulses and velocity ramps\n  Position section               Counts and manual references\n  Alarm section                  Four driver alarms\n  Command section                Commands and immediate O stop\nPi/gantry_keyboard.py            Hold-to-move keys on the computer\nPi/Pi_FarmBotController_Class.py  Typed commands on the computer"),
    ("page", ""),
    ("title", "From a command to movement"),
    ("h", "What happens when you send X1000"),
    ("p", "The Python controller sends X1000 followed by a newline over USB serial at 115200 baud. CommandProcessor checks that X is a real axis and 1000 is a valid signed number. MotorControl adds 1000 to X's current position to get the target, checks the stop latch, and sets the X direction pins. It then generates 1000 step cycles. In each cycle it raises both X pulse pins before lowering either, so M1 and M2 each receive 1000 pulses. X's stored position increases by 1000, rather than 2000, because the two motors represent one axis. Y and Z receive no pulses during that command."),
    ("p", "The motor engine now uses AF-434's Timer1 design: a 20,000 Hz interrupt generates pulses while a 1,000 Hz controller smoothly adjusts velocity. A pulse is held HIGH for the configured three microseconds. Default speed is 1000 steps/second, adjustable from 100 to 8000, and acceleration is 3000 steps/second squared. Release a key or press Space and velocity ramps down to zero; reversing first slows through zero, then accelerates the other way. At default speed a normal release takes roughly 0.33 seconds and 167 steps. For a finite move, remaining distance determines when braking begins; the final approach uses a low crawl speed and the pulse generator stops at the exact target count. Keyboard axes can run together; new finite position moves are accepted while idle."),
    ("h", "What the position number means"),
    ("p", "Position is a software count of the pulses the Arduino issued. X1000 changes X by +1000; Y-500 changes Y by -500. The Arduino does not read the motors' encoder positions over the alarm wires. The integrated drivers handle their own internal feedback. If a motor is moved by hand, power is lost, or motion is interrupted abruptly, the software count may no longer describe the physical location. An emergency stop therefore invalidates the manual position references. Clearing a stop allows new relative moves, but does not recreate a physical zero."),
    ("h", "No switches means a manual origin"),
    ("p", "Relative moves work immediately, without homing. To establish a physical reference, use small relative moves to reach a known origin, then send 0 to record all current axis positions as zero. Use 0X, 0Y, or 0Z to reference one axis. The command records the current location and produces no motion. H, HX, HY, HZ, and HALL only explain that homing is unavailable. Nothing searches for an end stop."),
    ("p", "Percentage positioning also requires a known travel length. X_TRAVEL_STEPS, Y_TRAVEL_STEPS, and Z_TRAVEL_STEPS near the top of AgriGantry.ino start at 0 because the measured usable distances are unknown. Set an axis's actual travel in pulses, upload, then manually establish its origin with 0 or its per-axis version. PX50 can then move X to half of that travel. Referenced relative moves are clipped to the configured range of 0 through travel. Until both the reference and travel exist, there are no physical software bounds; percentage moves are refused."),
    ("page", ""),
    ("title", "Keyboard movement and stopping"),
    ("h", "Hold a key, release to slow down"),
    ("p", "Pi/gantry_keyboard.py starts a global pynput listener. Hold W/S to move X, A/D for Y, and Q/E for Z. Releasing a key ramps that axis to rest, while other held axes keep moving. If opposite keys on one axis are held, the latest press wins; releasing it falls back to the other held key. Space requests a smooth stop on all axes. The controller sends a heartbeat every 100 milliseconds. If the Arduino receives no jog heartbeat for 500 milliseconds, it slows jogging to a stop. Typed finite moves finish without requiring heartbeats."),
    ("p", "O is the immediate emergency stop: it cancels every velocity and target instead of decelerating. It also works during an incomplete or overflowing serial command. All movement stays blocked until C in keyboard mode, or CLEAR/S0 in typed mode. A clear request succeeds only when the live motor alarms are normal. Release and press movement keys again after clearing; old held keys never restart by themselves. Drivers retain holding torque, including Z. This is a software stop requiring a functioning controller, USB connection, and Arduino."),
    ("h", "What happens when a motor reports an alarm"),
    ("p", "ALM pins 26, 27, 28, and 29 are monitored continuously. A HIGH input sustained for five milliseconds latches the same global stop as O and identifies the motor. Shorter transients do not latch. Returning LOW does not resume movement; clear the stop explicitly after resolving the fault. This uses AF-434's actual active-HIGH alarm behavior."),
    ("h", "Keyboard controls"),
    ("code", "W / S           X forward / backward, both motors\nA / D           Y left / right\nQ / E           Z up / down\nRelease key     Smooth stop on that axis\nSpace           Smooth stop on ALL axes\nO               Immediate latched EMERGENCY STOP\nC               Clear stop after fixing any alarm\n+ / -           Increase / decrease jog speed\nP               Show positions and velocities\n0               Record manual zero while stopped\nEsc             Smooth stop and exit"),
    ("p", "The listener captures keys even while another window has focus, so close keyboard control before typing elsewhere. Run one controller at a time. The typed interface still accepts X1000, Y-500, PX25/PY50/PZ75, 0X/0Y/0Z, STATUS, ALM, and HELP. A Serial Monitor may require Send/Enter to transmit O; the Python listeners send it on the actual keypress."),
    ("page", ""),
    ("title", "Running and understanding it"),
    ("h", "Build, upload, and run"),
    ("p", "Open Arduino/AgriGantry/AgriGantry.ino in Arduino IDE. This one file contains all the gantry firmware. Select Arduino Mega or Mega 2560, choose the Mega's port, and click Upload. Close Serial Monitor, then run the Python keyboard controller on your computer using the commands below from the repository root. Replace COM9 with your actual port; omit --port for automatic detection."),
    ("code", "python -m pip install -r Pi/requirements.txt\npython Pi/gantry_keyboard.py --port COM9\n\nOptional typed interface, after closing keyboard control:\npython Pi/Pi_FarmBotController_Class.py --port COM9"),
    ("p", "A desktop session with keyboard access and the listed packages is required. Startup pulses DTR to reset the Mega, clears old serial transfers, waits for boot, and verifies the firmware identity. Startup motor alarms are displayed and block keyboard movement. This reset sequence recovered the COM9 idle write timeout. Use --list-ports to find the Mega if needed. Esc or Ctrl+C requests a smooth stop before closing; after three seconds without confirmation it sends an emergency stop. Startup holds torque and issues no movement pulses."),
    ("h", "What was verified"),
    ("p", "The complete .ino compiled using Arduino IDE's build tool for the Mega 2560: 16,688 bytes of program storage and 548 bytes of static RAM. Native tests exercise the firmware against a simulated 20 kHz timer and pins, with 3,278 checks across unknown-travel and configured-travel builds. The same checks run directly against the single-file sketch with --standalone. They cover paired pulses, finite targets and braking, key-release slowing, reversal through zero, simultaneous axes, heartbeat loss, O stops, alarms, zero restrictions, and software travel bounds. Twenty-five Python tests cover key handling, stop states, startup reset and alarm preservation, serial errors, and firmware identification."),
    ("p", "Movement was checked through build and simulation tests. Live USB communication on COM9 passed two consecutive connections, firmware/status queries, and 80 idle heartbeat writes using the new reset sequence. No movement commands were sent during that check. No firmware was uploaded by the agent and physical movement remains untested. The commands below repeat the automated checks; the native runner uses MSVC."),
    ("code", "python Farm-Bot/test/run_native.py --standalone\npython -m unittest discover -s Pi -p 'test_*.py'\npython Farm-Bot/tools/make_guide.py"),
]


def escape(text):
    return text.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


class GuidePDF:
    def __init__(self):
        self.pages = []
        self.new_page()

    def text(self, x, y, value, font="F1", size=10.5, color="0.13 0.18 0.17"):
        self.pages[-1].append(f"BT /{font} {size} Tf {color} rg 1 0 0 1 {x} {y:.1f} Tm ({escape(value)}) Tj ET")

    def new_page(self):
        self.pages.append([])
        self.text(54, 752, "AGRI / GANTRY", "F2", 11, "0.12 0.38 0.25")
        self.text(400, 752, "PROGRAM GUIDE", "F1", 9)
        self.pages[-1].append("0.12 0.38 0.25 RG 1.5 w 54 740 m 558 740 l S")
        self.y = 710

    def add(self, kind, value):
        if kind == "page":
            self.new_page()
            return
        font, size, leading, gap, width = {
            "title": ("F2", 23, 29, 9, 40),
            "subtitle": ("F1", 10, 15, 10, 90),
            "h": ("F2", 13, 19, 6, 70),
            "p": ("F1", 10.5, 14, 9, 95),
            "code": ("F3", 9, 12.5, 10, 90),
        }[kind]
        lines = value.splitlines() if kind == "code" else textwrap.wrap(value, width=width)
        # Keep headings with at least two lines of the following paragraph.
        needed = len(lines) * leading + (35 if kind == "h" else 0)
        if self.y - needed < 60:
            self.new_page()
        for line in lines:
            if self.y < 60:
                self.new_page()
            self.text(54, self.y, line, font, size)
            self.y -= leading
        self.y -= gap

    def save(self, path):
        count = len(self.pages)
        objects = [b"<< /Type /Catalog /Pages 2 0 R >>", b"",
                   b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
                   b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>",
                   b"<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>"]
        page_ids = []
        for index, commands in enumerate(self.pages, 1):
            self.pages[index - 1].append(f"BT /F1 9 Tf 0.4 0.45 0.43 rg 1 0 0 1 54 34 Tm (AF-434 hardware / AgriGantry controls) Tj ET")
            commands.append(f"BT /F1 9 Tf 1 0 0 1 500 34 Tm ({index} / {count}) Tj ET")
            page_id = len(objects) + 1
            page_ids.append(page_id)
            objects.append(f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R /F2 4 0 R /F3 5 0 R >> >> /Contents {page_id + 1} 0 R >>".encode())
            stream = "\n".join(commands).encode("ascii")
            objects.append(f"<< /Length {len(stream)} >>\nstream\n".encode() + stream + b"\nendstream")
        objects[1] = f"<< /Type /Pages /Count {count} /Kids [{' '.join(f'{i} 0 R' for i in page_ids)}] >>".encode()
        result = bytearray(b"%PDF-1.4\n")
        offsets = [0]
        for number, obj in enumerate(objects, 1):
            offsets.append(len(result))
            result.extend(f"{number} 0 obj\n".encode() + obj + b"\nendobj\n")
        xref = len(result)
        result.extend(f"xref\n0 {len(objects) + 1}\n0000000000 65535 f \n".encode())
        for offset in offsets[1:]:
            result.extend(f"{offset:010d} 00000 n \n".encode())
        result.extend(f"trailer\n<< /Size {len(objects) + 1} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
        path.write_bytes(result)
        return count


def main():
    docs = ROOT / "docs"
    docs.mkdir(exist_ok=True)
    pdf = GuidePDF()
    for kind, value in DOC:
        pdf.add(kind, value)
    count = pdf.save(docs / "AgriGantry_Program_Guide.pdf")
    (docs / "AgriGantry_Program_Guide.txt").write_text(
        "\n\n".join(value for kind, value in DOC if kind != "page") + "\n", encoding="utf-8")
    print(f"Generated {count}-page PDF and plain-text guide in {docs}")


if __name__ == "__main__":
    main()
