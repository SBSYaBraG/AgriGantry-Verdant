"""Tests use fake serial/keyboard devices; they do not open a port or hook keys."""
import contextlib
import importlib.util
import io
from pathlib import Path
import threading
import types
import unittest
from unittest.mock import patch

serial_stub = types.ModuleType("serial")
serial_stub.SerialException = OSError
serial_stub.tools = types.ModuleType("serial.tools")
serial_stub.tools.list_ports = types.ModuleType("serial.tools.list_ports")
serial_stub.tools.list_ports.comports = lambda: []
pynput_stub = types.ModuleType("pynput")
pynput_stub.keyboard = types.SimpleNamespace()
spec = importlib.util.spec_from_file_location("farmbot_controller", Path(__file__).with_name("Pi_FarmBotController_Class.py"))
module = importlib.util.module_from_spec(spec)
with patch.dict("sys.modules", {"serial": serial_stub, "serial.tools": serial_stub.tools,
                               "serial.tools.list_ports": serial_stub.tools.list_ports,
                               "pynput": pynput_stub}):
    spec.loader.exec_module(module)


class Connection:
    def __init__(self):
        self.writes = []
        self.closed = False

    def write(self, data):
        self.writes.append(data)
        return len(data)

    def close(self):
        self.closed = True


class ControllerTests(unittest.TestCase):
    def setUp(self):
        self.output = contextlib.redirect_stdout(io.StringIO())
        self.output.__enter__()
        self.controller = module.FarmBotController()
        self.connection = Connection()
        self.controller.arduino = self.connection

    def tearDown(self):
        self.output.__exit__(None, None, None)

    def press(self, char):
        self.controller.on_key_press(types.SimpleNamespace(char=char))

    def test_lowercase_o_sends_immediate_stop(self):
        self.press("o")
        self.assertEqual(self.connection.writes, [b"O\n"])
        self.assertTrue(self.controller.emergency_stop_active)

    def test_uppercase_o_and_repeat_guard(self):
        self.press("O")
        self.press("O")
        self.assertEqual(self.connection.writes, [b"O\n"])
        self.controller.on_key_release(types.SimpleNamespace(char="O"))
        self.press("o")
        self.assertEqual(self.connection.writes, [b"O\n", b"O\n"])

    def test_other_keys_do_not_stop(self):
        for char in (None, "x", "c", "0"):
            self.press(char)
        self.assertFalse(self.controller.emergency_stop_active)
        self.assertEqual(self.connection.writes, [])

    def test_listener_registers_key_callbacks(self):
        calls = []

        class Listener:
            def __init__(listener, **callbacks):
                self.assertEqual(callbacks["on_press"], self.controller.on_key_press)
                self.assertEqual(callbacks["on_release"], self.controller.on_key_release)

            def start(listener): calls.append("start")
            def wait(listener): calls.append("ready")
            def stop(listener): calls.append("stop")

        with patch.object(module, "keyboard", types.SimpleNamespace(Listener=Listener)):
            self.assertTrue(self.controller.start_emergency_listener())
            self.controller.cleanup()
        self.assertEqual(calls, ["start", "ready", "stop"])

    def test_missing_keyboard_dependency_is_reported(self):
        with patch.object(module, "keyboard", None):
            self.assertFalse(self.controller.start_emergency_listener())

    def test_hotkey_can_write_while_command_waits_for_response(self):
        waiting = threading.Event()
        release = threading.Event()

        class BlockingConnection(Connection):
            @property
            def in_waiting(connection):
                waiting.set()
                release.wait(3)
                raise OSError("End simulated response wait")

        connection = BlockingConnection()
        self.controller.arduino = connection
        worker = threading.Thread(target=self.controller.send_command, args=("X1000",))
        with patch.object(module.time, "sleep", lambda _: None):
            worker.start()
            try:
                self.assertTrue(waiting.wait(2))
                self.assertTrue(worker.is_alive())
                self.press("o")
                self.assertEqual(connection.writes, [b"X1000\n", b"O\n"])
            finally:
                release.set()
                worker.join(2)
        self.assertFalse(worker.is_alive())

    def test_cleanup_stops_before_closing_port(self):
        self.controller.cleanup()
        self.assertEqual(self.connection.writes, [b"O\n"])
        self.assertTrue(self.connection.closed)
        self.assertIsNone(self.controller.arduino)


if __name__ == "__main__":
    unittest.main()
