"""Hold-to-move AgriGantry controls, adapted from the working AF-434 controller.

Run from the repository root:
    python Pi/gantry_keyboard.py --port COM9

W/S: X forward/backward; A/D: Y left/right; Q/E: Z up/down.
Release a key to decelerate. Space smoothly stops all axes. O emergency-stops
without a ramp and stays latched until C. +/- changes speed; P reports status;
0 records the manual origin while stopped; Esc smoothly stops and exits.

Keys are captured globally while this controller runs.
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

FIRMWARE_ID = "AGRIGANTRY-KEYBOARD-v1"
HEARTBEAT_INTERVAL = 0.1
JOG_KEYS = {
    "w": ("X", 1), "s": ("X", -1),
    "d": ("Y", 1), "a": ("Y", -1),
    "q": ("Z", 1), "e": ("Z", -1),
}
SPEED_KEYS = {"+": "plus", "=": "plus", "-": "minus", "_": "minus"}
KEY_HELP = """\
Hold W/S: X forward/backward   A/D: Y left/right   Q/E: Z up/down
Release a key: smooth stop     Space: smooth stop all
O: EMERGENCY STOP all          C: clear stop after fixing any alarm
+/-: change speed             P: status    0: manual zero while stopped
Esc: smooth stop and exit. Keys are captured globally.
"""


def normalize(key):
    if key == keyboard.Key.esc:
        return "esc"
    if key == keyboard.Key.space:
        return "space"
    char = getattr(key, "char", None)
    if not char:
        return None
    if char in SPEED_KEYS:
        return SPEED_KEYS[char]
    name = char.lower()
    return name if name in JOG_KEYS or name in ("o", "c", "p", "0") else None


class KeyboardController:
    def __init__(self, connection):
        self.connection = connection
        self.lock = threading.RLock()
        self.pressed = set()
        self.held = []
        self.blocked_until_release = set()
        self.estopped = False
        self.exiting = False

    def send(self, command):
        # Callers hold self.lock; complete packets cannot interleave.
        try:
            self.connection.write((command + "\n").encode("ascii"))
        except serial.SerialException as exc:
            port = getattr(self.connection, "port", "serial port")
            raise serial.SerialException(
                f"{port} failed while sending {command!r}: {exc}. "
                "Close Serial Monitor, reconnect the USB cable, and run again."
            ) from exc

    def heartbeat(self):
        with self.lock:
            if not self.exiting:
                self.send("HB")

    def discard_held_motion(self):
        self.blocked_until_release.update(self.pressed.intersection(JOG_KEYS))
        self.held.clear()

    def stop_all(self, emergency=False):
        with self.lock:
            self.discard_held_motion()
            if emergency:
                self.estopped = True
            self.send("O" if emergency else "J0")

    def begin_exit(self):
        with self.lock:
            self.exiting = True
            self.discard_held_motion()
            self.send("J0")

    def handle_response(self, text):
        with self.lock:
            if (text.startswith("EMERGENCY STOP ACTIVATED") or text.startswith("ALARM:")
                    or text.startswith("ERR emergency stop active")
                    or text == "EMERGENCY STOP active"):
                self.estopped = True
                self.discard_held_motion()
            elif text.startswith("Emergency stop cleared"):
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
                self.begin_exit()
                return False
            if name == "o":
                self.stop_all(emergency=True)
                print("O: EMERGENCY STOP. Release movement keys; press C to clear.")
            elif name == "space":
                self.stop_all()
                print("Space: slowing all axes to a stop")
            elif name == "c":
                self.send("CLEAR")
            elif name == "p":
                self.send("R")
            elif name == "0":
                self.send("0")
            elif name in ("plus", "minus"):
                self.send("J+" if name == "plus" else "J-")
            elif name in JOG_KEYS:
                if self.estopped or name in self.blocked_until_release:
                    self.blocked_until_release.add(name)
                    return None
                self.held.append(name)
                axis, direction = JOG_KEYS[name]
                self.send(f"J{axis}{direction}")
        return None

    def on_release(self, key):
        name = normalize(key)
        if name is None:
            return
        with self.lock:
            self.pressed.discard(name)
            self.blocked_until_release.discard(name)
            if name not in self.held:
                return
            self.held.remove(name)
            if self.exiting or self.estopped:
                return
            axis, _ = JOG_KEYS[name]
            other = next((k for k in reversed(self.held) if JOG_KEYS[k][0] == axis), None)
            self.send(f"J{axis}{JOG_KEYS[other][1] if other else 0}")


def find_port():
    ports = list(serial.tools.list_ports.comports())
    candidates = [p for p in ports if p.vid in (0x2341, 0x2A03, 0x1A86)
                  or any(tag in (p.description or "") for tag in ("Arduino", "Mega", "CH340", "USB Serial"))]
    if len(candidates) == 1:
        return candidates[0].device
    listing = "\n".join(f"  {p.device}: {p.description}" for p in ports) or "  No serial ports found."
    raise SystemExit(f"Choose the Mega with --port COMx. Available ports:\n{listing}")


def prepare_connection(connection):
    """Pulse DTR and clear old transfers before talking to a reset Mega.

    Reopening COM9 alone did not reliably reset this board after a Windows
    write timeout. An explicit falling/rising DTR transition restored replies.
    """
    connection.dtr = False
    time.sleep(0.15)
    connection.reset_input_buffer()
    connection.reset_output_buffer()
    connection.dtr = True
    time.sleep(2.5)


def verify_firmware(connection):
    connection.write(b"?\n")
    deadline = time.monotonic() + 2
    startup_lines = []
    while time.monotonic() < deadline:
        text = connection.readline().decode("ascii", errors="replace").strip()
        if text == FIRMWARE_ID:
            return startup_lines
        if text:
            startup_lines.append(text)
    raise SystemExit(f"Upload the current Farm-Bot firmware first; expected {FIRMWARE_ID}.")


class ResponseReader:
    def __init__(self):
        self.pending = b""

    def poll(self, connection, controller):
        self.pending += connection.read(connection.in_waiting)
        lines = self.pending.split(b"\n")
        self.pending = lines[-1]
        stopped = False
        for line in lines[:-1]:
            text = line.decode("ascii", errors="replace").strip()
            if text:
                controller.handle_response(text)
                print(f"[Mega] {text}")
                stopped = stopped or text == "STOPPED"
        return stopped


def run(port):
    if keyboard is None:
        raise SystemExit("Install Pi/requirements.txt and run in a desktop session for keyboard access.")
    with serial.Serial(port, 115200, timeout=0.1, write_timeout=1,
                       xonxoff=False, rtscts=False, dsrdtr=False) as connection:
        prepare_connection(connection)
        startup_lines = verify_firmware(connection)
        controller = KeyboardController(connection)
        for line in startup_lines:
            controller.handle_response(line)
            print(f"[Mega] {line}")
        listener = keyboard.Listener(on_press=controller.on_press, on_release=controller.on_release)
        reader = ResponseReader()
        print(f"Connected to {port}.\n{KEY_HELP}")
        if controller.estopped:
            print("Motor alarm / emergency stop is active. Resolve alarms, then press C.")
        try:
            listener.start()
            listener.wait()
            while listener.is_alive():
                listener.join(HEARTBEAT_INTERVAL)
                controller.heartbeat()
                reader.poll(connection, controller)
        finally:
            listener.stop()
            try:
                controller.begin_exit()
                deadline = time.monotonic() + 3
                stopped = False
                while time.monotonic() < deadline and not stopped:
                    time.sleep(0.05)
                    stopped = reader.poll(connection, controller)
                if not stopped:
                    controller.stop_all(emergency=True)
            except serial.SerialException:
                pass  # firmware's 500ms jog timeout also ramps to a stop
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
