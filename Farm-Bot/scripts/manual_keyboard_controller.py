"""Keyboard controller for the separate manual-calibration Mega firmware.

Run from Farm-Bot after uploading the megaatmega2560-manual-calibration target:
    python scripts/manual_keyboard_controller.py --port COM9

F1/F2/F3 select X/Y/Z. C begins calibration of the selected axis. During
calibration hold A/D for X, Left/Right for Y, and W/S for Z. Enter confirms an
endpoint after its two-second prompt; Backspace rejects that prompt. After an
axis is calibrated: 1/2/3/4/5 move it to 10/25/50/75/90%, H moves the selected
axis to its stored 50% midpoint, and G returns all calibrated axes to 50%.
Space requests a controlled stop; O or Esc latches
an emergency stop. L clears a stop only after every ALM line is normal.
"""

import argparse
import sys
import threading
import time

import serial
import serial.tools.list_ports

try:
    from pynput import keyboard
except (ImportError, OSError):
    keyboard = None


FIRMWARE_ID = "FARMBOT-MANUAL-CAL-v1"
HEARTBEAT_INTERVAL = 0.10
PERCENT_KEYS = {"1": 10, "2": 25, "3": 50, "4": 75, "5": 90}
JOG_KEYS = {
    "a": ("X", -1), "d": ("X", 1),
    "w": ("Z", 1), "s": ("Z", -1),
    "left": ("Y", -1), "right": ("Y", 1),
}
HELP = """\
F1/F2/F3: select X/Y/Z       C: begin/repeat manual calibration
Hold A/D: X                  Hold Left/Right: Y       Hold W/S: Z
Enter: confirm endpoint      Backspace: reject endpoint prompt
1/2/3/4/5: 10/25/50/75/90%  H: selected axis to 50%   G: all axes to 50%
Space: controlled stop       O or Esc: EMERGENCY STOP  L: clear after ALM is OK
P: status. The controller must remain running while a jog key is held.
"""


def normalize(key):
    special = {
        keyboard.Key.left: "left", keyboard.Key.right: "right",
        keyboard.Key.space: "space", keyboard.Key.esc: "esc",
        keyboard.Key.enter: "enter", keyboard.Key.backspace: "backspace",
        keyboard.Key.f1: "f1", keyboard.Key.f2: "f2", keyboard.Key.f3: "f3",
    }
    if key in special:
        return special[key]
    char = getattr(key, "char", None)
    return char.lower() if char else None


class KeyboardController:
    def __init__(self, connection):
        self.connection = connection
        self.lock = threading.RLock()
        self.pressed = set()
        self.held_motion = set()
        self.selected_axis = "X"
        self.estopped = False
        self.exiting = False

    def send(self, command):
        self.connection.write((command + "\n").encode("ascii"))

    def heartbeat(self):
        with self.lock:
            if self.held_motion and not self.estopped and not self.exiting:
                self.send("HB")

    def stop_motion(self, emergency=False):
        self.held_motion.clear()
        self.pressed.difference_update(JOG_KEYS)
        if emergency:
            self.estopped = True
            self.send("O")
        else:
            self.send("STOP")

    def select(self, axis):
        self.selected_axis = axis
        self.send(f"SELECT {axis}")
        print(f"Selected {axis}")

    def handle_response(self, message):
        if message.startswith(("EMERGENCY_STOP", "ALARM_LATCHED")):
            with self.lock:
                self.estopped = True
                self.held_motion.clear()
        elif message.startswith("EMERGENCY_CLEARED"):
            with self.lock:
                self.estopped = False

    def on_press(self, key):
        name = normalize(key)
        if name is None:
            return None
        with self.lock:
            if self.exiting or name in self.pressed:
                return None
            self.pressed.add(name)
            if name == "esc":
                self.stop_motion(emergency=True)
                self.exiting = True
                return False
            if name == "o":
                self.stop_motion(emergency=True)
            elif name == "space":
                self.stop_motion()
            elif name == "f1":
                self.select("X")
            elif name == "f2":
                self.select("Y")
            elif name == "f3":
                self.select("Z")
            elif name == "c":
                self.send(f"CAL {self.selected_axis}")
            elif name == "h":
                self.send(f"HOME {self.selected_axis}")
            elif name == "g":
                self.send("HOME ALL")
            elif name == "enter":
                self.send("CONFIRM")
            elif name == "backspace":
                self.send("CANCEL")
            elif name == "l":
                self.send("CLEAR")
            elif name == "p":
                self.send("STATUS")
            elif name in PERCENT_KEYS:
                self.send(f"P{self.selected_axis}{PERCENT_KEYS[name]}")
            elif name in JOG_KEYS:
                if self.estopped:
                    print("Emergency stop is latched. Fix the fault, then press L.")
                    return None
                axis, direction = JOG_KEYS[name]
                self.held_motion.add(name)
                self.send(f"J{axis}{direction}")
        return None

    def on_release(self, key):
        name = normalize(key)
        if name is None:
            return
        with self.lock:
            self.pressed.discard(name)
            if name not in self.held_motion:
                return
            self.held_motion.remove(name)
            if not self.estopped and not self.exiting:
                axis, _ = JOG_KEYS[name]
                self.send(f"J{axis}0")


def find_port():
    ports = list(serial.tools.list_ports.comports())
    candidates = [port for port in ports if port.vid in (0x2341, 0x2A03, 0x1A86)
                  or any(tag in (port.description or "") for tag in
                         ("Arduino", "Mega", "CH340", "USB Serial"))]
    if len(candidates) == 1:
        return candidates[0].device
    listing = "\n".join(f"  {port.device}: {port.description}" for port in ports) or "  No serial ports found."
    raise SystemExit(f"Choose the Mega with --port COMx. Available ports:\n{listing}")


def prepare_connection(connection):
    # Opening the COM port can reset a Mega. Firmware remains safely disabled
    # throughout reset, then this waits for its serial startup messages.
    connection.dtr = False
    time.sleep(0.15)
    connection.reset_input_buffer()
    connection.reset_output_buffer()
    connection.dtr = True
    time.sleep(2.5)


def verify_firmware(connection):
    connection.write(b"?\n")
    deadline = time.monotonic() + 2
    startup = []
    while time.monotonic() < deadline:
        message = connection.readline().decode("ascii", errors="replace").strip()
        if message == FIRMWARE_ID:
            return startup
        if message:
            startup.append(message)
    raise SystemExit(f"Expected {FIRMWARE_ID}. Upload the manual-calibration target first.")


def run(port):
    if keyboard is None:
        raise SystemExit("Install scripts/requirements-manual.txt in a desktop Python environment.")
    with serial.Serial(port, 115200, timeout=0.1, write_timeout=1,
                       xonxoff=False, rtscts=False, dsrdtr=False) as connection:
        prepare_connection(connection)
        startup = verify_firmware(connection)
        controller = KeyboardController(connection)
        for message in startup:
            controller.handle_response(message)
            print(f"[Mega] {message}")
        print(f"Connected to {port}.\n{HELP}")
        listener = keyboard.Listener(on_press=controller.on_press, on_release=controller.on_release)
        listener.start()
        pending = b""
        try:
            while listener.is_alive():
                listener.join(HEARTBEAT_INTERVAL)
                controller.heartbeat()
                pending += connection.read(connection.in_waiting)
                lines = pending.split(b"\n")
                pending = lines[-1]
                for line in lines[:-1]:
                    message = line.decode("ascii", errors="replace").strip()
                    if message:
                        controller.handle_response(message)
                        print(f"[Mega] {message}")
        finally:
            listener.stop()
            try:
                controller.stop_motion(emergency=True)
            except serial.SerialException:
                pass
    print("Keyboard controller closed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="Mega serial port, such as COM9")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()
    if args.list_ports:
        for port in serial.tools.list_ports.comports():
            print(f"{port.device}: {port.description}")
        return
    try:
        run(args.port or find_port())
    except KeyboardInterrupt:
        pass
    except serial.SerialException as exc:
        raise SystemExit(f"Serial connection failed: {exc}") from exc


if __name__ == "__main__":
    main()
