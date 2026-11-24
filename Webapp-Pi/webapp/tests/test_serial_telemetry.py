import time
import unittest
from unittest.mock import patch

from app import create_app
from hardware.arduino import Arduino
from hardware.stm32 import STM32
from hardware.telemetry import SerialTelemetry


class Replies:
    def __init__(self, **responses):
        self.responses, self.sent = responses, []

    def replies(self, cmd):
        self.sent.append(cmd)
        yield from self.responses[cmd]
        raise TimeoutError(f"Incomplete {cmd}")


class TelemetryTests(unittest.TestCase):
    def setUp(self):
        self.stm = STM32()
        self.hw = SerialTelemetry(self.stm)
        self.link = Replies(PING=["PONG"], STATUS=["STATUS uptime_ms=900 ENA_X=0 ENA_Y=0 ENA_Z=0 ENC_X1=-3999"])

    def poll_once(self):
        with patch.object(self.hw.shutdown, "wait", side_effect=lambda _: self.hw.shutdown.set()):
            self.hw._poll(self.link, self.stm.identify(self.link))
        self.hw.shutdown.clear()

    def test_socket_traffic_does_not_refresh_timestamps_and_no_implicit_scan(self):
        self.poll_once()
        a, b = self.hw.get_status(), self.hw.get_status()
        self.assertEqual(a, b)
        self.assertTrue(a["hardware_connected"])
        self.assertEqual(a["encoder_x1"], -3999)
        self.assertEqual(a["axes"], {})
        self.assertEqual(self.link.sent, ["PING", "STATUS"])
        with patch("hardware.telemetry.time.monotonic", return_value=time.monotonic() + 4):
            self.assertFalse(self.hw.get_status()["hardware_connected"])
            self.assertFalse(self.hw.request_scan()["ok"])
        self.hw._failed(TimeoutError("No reply"))
        self.assertFalse(self.hw.get_status()["hardware_connected"])
        self.assertEqual(self.hw.get_status()["sample_time"], a["sample_time"])

    def test_explicit_scan_reports_each_driver_and_separates_transient_from_communication(self):
        self.poll_once()
        self.link.responses["SCAN"] = [
            f"SCAN {name} SPI=OK FAULT=0x80/0x00 nFAULT=0/1/1 STATUS_BITS=0x00 CTRL1=0x0F CTRL2=0x06"
            for name in self.stm.drivers
        ]
        self.assertTrue(self.hw.request_scan()["ok"])
        self.assertFalse(self.hw.request_scan()["ok"])
        self.poll_once()
        self.assertEqual(self.link.sent.count("SCAN"), 1)
        result = self.hw.get_status()["scan"]["drivers"]
        self.assertEqual(list(result), list(self.stm.drivers))
        self.assertEqual(result["U5/X2"]["spi"], "OK")
        self.assertEqual((result["U5/X2"]["first"], result["U5/X2"]["settled"]), ("0x80", "0x00"))
        self.link.responses["SCAN"][1] = self.link.responses["SCAN"][0]
        with self.assertRaises(ValueError):
            self.stm.scan(self.link)

    def test_high_enable_cancels_a_queued_scan(self):
        self.poll_once()
        self.assertTrue(self.hw.request_scan()["ok"])
        self.link.responses["STATUS"] = ["STATUS uptime_ms=901 ENA_X=1 ENA_Y=0 ENA_Z=0 ENC_X1=NA"]
        self.poll_once()
        self.assertNotIn("SCAN", self.link.sent)
        self.assertIn("error", self.hw.get_status()["scan"])

    def test_arduino_profiles_require_identity_and_complete_axis_data(self):
        p = Arduino()
        with self.assertRaises(TimeoutError):
            p.identify(Replies(**{"?": ["unrelated device"]}))
        p.identify(Replies(**{"?": [p.manual]}))
        status = ["MOTION active=- | velocity=0 | estop=no"]
        status += [f"{axis}: travel=2000 | raw_position=-500 | ready=YES" for axis in "XYZ"]
        status += [f"{m}: OK | latched=no" for m in ("X-left", "X-right", "Y", "Z")]
        sample = p.sample(Replies(STATUS=status))
        self.assertEqual(sample["axes"]["x"]["percent"], 25)
        self.assertEqual(sample["alm"], dict(XL=False, XR=False, Y=False, Z=False))
        status[-1] = "Z: OK | latched=YES"
        self.assertTrue(p.sample(Replies(STATUS=status))["alm"]["Z"])
        with self.assertRaises(TimeoutError):
            p.sample(Replies(STATUS=status[:-1]))
        p.identify(Replies(**{"?": [p.arturo]}))
        sample = p.sample(Replies(STATUS=[f"{a}: position=12 | percent=25 | manual zero not set" for a in "XYZ"]))
        self.assertFalse(sample["axes"]["x"]["homed"])
        self.assertNotIn("alm", sample)

    def test_profiles_refuse_browser_motion_and_return_controller_status_over_http(self):
        for backend in (self.hw, SerialTelemetry(Arduino())):
            app, socket = create_app(backend)
            client = socket.test_client(app)
            for name in ("move_relative", "move_absolute", "home_axis", "home_all", "emergency_stop", "resume"):
                self.assertFalse(client.emit(name, {"axis": "x"}, callback=True)["ok"])
            self.assertFalse(client.emit("scan_drivers", callback=True)["ok"])
            snapshot = app.test_client().get("/api/status").get_json()
            self.assertEqual(snapshot["mode"], backend.mode)
            self.assertIsNone(snapshot["sample_time"])
            self.assertFalse(snapshot["hardware_connected"])
            client.disconnect()
