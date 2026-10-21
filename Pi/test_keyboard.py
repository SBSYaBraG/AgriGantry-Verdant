"""Exercise keyboard events and serial packets without hooking real keys."""
import contextlib
import importlib.util
import io
from pathlib import Path
import types
import unittest
from unittest.mock import patch

serial_stub = types.ModuleType("serial")
serial_stub.SerialException = OSError
serial_stub.tools = types.ModuleType("serial.tools")
serial_stub.tools.list_ports = types.ModuleType("serial.tools.list_ports")
serial_stub.tools.list_ports.comports = lambda: []
pynput_stub = types.ModuleType("pynput")
pynput_stub.keyboard = types.SimpleNamespace(Key=types.SimpleNamespace(esc=object(), space=object()))
spec = importlib.util.spec_from_file_location("gantry_keyboard_tested", Path(__file__).with_name("gantry_keyboard.py"))
module = importlib.util.module_from_spec(spec)
with patch.dict("sys.modules", {"serial": serial_stub, "serial.tools": serial_stub.tools,
                               "serial.tools.list_ports": serial_stub.tools.list_ports,
                               "pynput": pynput_stub}):
    spec.loader.exec_module(module)


class Connection:
    def __init__(self):
        self.writes = []
        self.received = b""

    def write(self, data): self.writes.append(data)
    @property
    def in_waiting(self): return len(self.received)
    def read(self, count):
        result, self.received = self.received[:count], self.received[count:]
        return result


class KeyboardTests(unittest.TestCase):
    def setUp(self):
        self.output = contextlib.redirect_stdout(io.StringIO())
        self.output.__enter__()
        self.connection = Connection()
        self.controller = module.KeyboardController(self.connection)

    def tearDown(self):
        self.output.__exit__(None, None, None)

    def key(self, char): return types.SimpleNamespace(char=char)
    def press(self, char): return self.controller.on_press(self.key(char))
    def release(self, char): self.controller.on_release(self.key(char))

    def test_all_axis_keys_and_releases(self):
        for key, axis, direction in (("w", "X", 1), ("s", "X", -1), ("a", "Y", -1),
                                     ("d", "Y", 1), ("q", "Z", 1), ("e", "Z", -1)):
            self.press(key.upper())
            self.release(key)
            self.assertEqual(self.connection.writes[-2:], [f"J{axis}{direction}\n".encode(), f"J{axis}0\n".encode()])

    def test_repeat_does_not_restart_ramps(self):
        self.press("w")
        self.press("w")
        self.assertEqual(self.connection.writes, [b"JX1\n"])

    def test_opposite_key_precedence_and_fallback(self):
        self.press("w")
        self.press("s")
        self.release("s")
        self.release("w")
        self.assertEqual(self.connection.writes, [b"JX1\n", b"JX-1\n", b"JX1\n", b"JX0\n"])

    def test_axes_can_be_held_together(self):
        for key in ("w", "d", "q"): self.press(key)
        self.release("w")
        self.assertEqual(self.connection.writes, [b"JX1\n", b"JY1\n", b"JZ1\n", b"JX0\n"])
        self.assertEqual(self.controller.held, ["d", "q"])

    def test_space_ramps_and_requires_a_fresh_press(self):
        self.press("w")
        self.controller.on_press(module.keyboard.Key.space)
        self.press("w")
        self.assertEqual(self.connection.writes, [b"JX1\n", b"J0\n"])
        self.assertFalse(self.controller.estopped)
        self.release("w")
        self.press("w")
        self.assertEqual(self.connection.writes[-1], b"JX1\n")

    def test_o_latches_and_c_waits_for_firmware_confirmation(self):
        self.press("O")
        self.press("w")
        self.press("c")
        self.assertTrue(self.controller.estopped)
        self.assertEqual(self.connection.writes, [b"O\n", b"CLEAR\n"])
        self.controller.handle_response("Cannot clear: a driver ALM input is still active")
        self.assertTrue(self.controller.estopped)
        self.controller.handle_response("Emergency stop cleared - operations resumed")
        self.assertFalse(self.controller.estopped)

    def test_held_keys_cannot_resume_after_clearing_o(self):
        self.press("w")
        self.press("d")
        self.press("o")
        self.controller.handle_response("Emergency stop cleared - operations resumed")
        self.press("w")
        self.release("d")
        self.assertEqual(self.connection.writes, [b"JX1\n", b"JY1\n", b"O\n"])
        self.release("w")
        self.press("w")
        self.assertEqual(self.connection.writes[-1], b"JX1\n")

    def test_driver_alarm_blocks_keyboard_motion(self):
        self.press("w")
        self.controller.handle_response("ALARM: Z (M4)")
        self.press("d")
        self.release("w")
        self.assertTrue(self.controller.estopped)
        self.assertEqual(self.connection.writes, [b"JX1\n"])

    def test_firmware_refusal_updates_keyboard_stop_state(self):
        self.controller.handle_response("ERR emergency stop active; send CLEAR")
        self.press("w")
        self.assertTrue(self.controller.estopped)
        self.assertEqual(self.connection.writes, [])

    def test_speed_shift_variants_and_status(self):
        self.press("+")
        self.release("=")
        self.press("=")
        self.press("-")
        self.press("p")
        self.press("0")
        self.assertEqual(self.connection.writes, [b"J+\n", b"J+\n", b"J-\n", b"R\n", b"0\n"])

    def test_escape_ramps_and_ends_keyboard_motion(self):
        self.press("w")
        self.assertIs(self.controller.on_press(module.keyboard.Key.esc), False)
        self.press("d")
        self.controller.heartbeat()
        self.assertEqual(self.connection.writes, [b"JX1\n", b"J0\n"])
        self.assertTrue(self.controller.exiting)

    def test_heartbeat_packet(self):
        self.controller.heartbeat()
        self.assertEqual(self.connection.writes, [b"HB\n"])

    def test_partial_serial_responses_update_stop_state(self):
        reader = module.ResponseReader()
        self.connection.received = b"EMERGENCY STOP ACT"
        self.assertFalse(reader.poll(self.connection, self.controller))
        self.assertFalse(self.controller.estopped)
        self.connection.received = b"IVATED - pulses stopped\nSTOPPED\n"
        self.assertTrue(reader.poll(self.connection, self.controller))
        self.assertTrue(self.controller.estopped)
        self.connection.received = b"Emergency stop cleared - operations resumed\n"
        reader.poll(self.connection, self.controller)
        self.assertFalse(self.controller.estopped)

    def test_unrelated_keys_are_ignored(self):
        self.press("b")
        self.controller.on_press(object())
        self.assertEqual(self.connection.writes, [])

    def test_firmware_identity_is_required_before_keyboard_control(self):
        connection = types.SimpleNamespace(reset_input_buffer=lambda: None,
                                           write=self.connection.write,
                                           readline=lambda: (module.FIRMWARE_ID + "\n").encode())
        module.verify_firmware(connection)
        self.assertEqual(self.connection.writes, [b"?\n"])
        connection.readline = lambda: b"AF434-GANTRY-v1\n"
        with patch.object(module.time, "monotonic", side_effect=[0, 1, 3]):
            with self.assertRaises(SystemExit): module.verify_firmware(connection)

    def test_startup_explicitly_resets_dtr_and_clears_transfers(self):
        events = []

        class ResetConnection:
            @property
            def dtr(connection): return True
            @dtr.setter
            def dtr(connection, value): events.append(("dtr", value))
            def reset_input_buffer(connection): events.append("clear_input")
            def reset_output_buffer(connection): events.append("clear_output")

        with patch.object(module.time, "sleep", side_effect=lambda seconds: events.append(("sleep", seconds))):
            module.prepare_connection(ResetConnection())
        self.assertEqual(events, [("dtr", False), ("sleep", 0.15), "clear_input", "clear_output",
                                  ("dtr", True), ("sleep", 2.5)])

    def test_startup_alarms_are_preserved_through_handshake(self):
        responses = iter([b"EMERGENCY STOP ACTIVATED\n", b"ALARM: Y (M3)\n",
                          (module.FIRMWARE_ID + "\n").encode()])
        connection = types.SimpleNamespace(write=self.connection.write, readline=lambda: next(responses))
        startup = module.verify_firmware(connection)
        for text in startup:
            self.controller.handle_response(text)
        self.assertEqual(startup, ["EMERGENCY STOP ACTIVATED", "ALARM: Y (M3)"])
        self.assertTrue(self.controller.estopped)
        self.press("w")
        self.assertEqual(self.connection.writes, [b"?\n"])

    def test_failed_write_identifies_command_and_is_not_retried(self):
        attempts = []

        def failing_write(data):
            attempts.append(data)
            raise OSError("Write timeout")

        self.connection.write = failing_write
        self.connection.port = "COM9"
        with self.assertRaisesRegex(OSError, "COM9 failed while sending 'JX1'"):
            self.press("w")
        self.assertEqual(attempts, [b"JX1\n"])


if __name__ == "__main__":
    unittest.main()
