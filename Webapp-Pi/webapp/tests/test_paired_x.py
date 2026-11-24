import time
import unittest
from unittest.mock import patch

from app import create_app
from hardware.paired_x import PairedXBench, decode_result
from hardware.stm32 import STM32
from hardware.telemetry import SerialTelemetry


READY = "PAIR X v=1 state=READY"
USED = "PAIR X v=1 state=USED"
ARMED = "ARMED X window_ms=30000"
DONE = "RESULT X run=DONE steps=3200 enc_delta=-3999 z_delta=1 ab_range=OK ms=19728"
STOPPED = "RESULT X run=STOPPED steps=17 enc_delta=-19 z_delta=0 ab_range=NA ms=243"


def report(driver):
    return f"DRV {driver} cfg=1 ctrl1=0x8F n=34 fault=0xC000 diag2=0xC000 reg=0x02 hal=0 raw=0xC000"


class BenchLink:
    def __init__(self):
        self.commands, self.bytes = [], []
        self.capability, self.ack, self.saved = READY, ARMED, DONE
        self.on_running = self.after_arm = None
        self.lose_ack = False

    def replies(self, cmd):
        self.commands.append(cmd)
        if cmd == "PAIR?":
            if self.capability is None:
                raise TimeoutError("Unknown command")
            yield self.capability
        elif cmd == "ARM X":
            if self.after_arm:
                self.after_arm()
            yield self.ack
        elif cmd == "STOP":
            yield "STOP X1 idle"
        elif cmd == "RESULT":
            yield from (self.saved, report("X1"), report("X2"))
        else:
            raise AssertionError(cmd)

    def write(self, value):
        self.bytes.append(value)

    def lines(self, timeout, service):
        assert timeout > 20
        if self.lose_ack:
            raise TimeoutError("Start acknowledgement lost")
        yield "MOVE X START"
        service()
        if self.on_running:
            self.on_running()
        service()
        self.saved = STOPPED if b"!" in self.bytes else DONE
        yield from (self.saved, report("X1"), report("X2"))


class PairedXTests(unittest.TestCase):
    def setUp(self):
        self.hw = SerialTelemetry(STM32(), paired_x=True)
        self.hw.error = None
        self.hw.received_tick = time.monotonic()
        self.hw.sample = dict(enables=[0, 0, 0], encoder_x1=0, uptime_ms=100)
        self.bench, self.link = self.hw.bench, BenchLink()
        self.bench.connect(self.link)

    def queue(self):
        token = self.hw.paired_request("arm", "owner", None)["token"]
        self.assertTrue(self.hw.paired_request("move", "owner", token)["ok"])
        self.assertTrue(self.bench.take_request())
        return token

    def test_two_phone_actions_one_serial_move_and_both_independent_results(self):
        app, socket = create_app(self.hw)
        owner, other = socket.test_client(app), socket.test_client(app)
        page = app.test_client().get("/")
        self.assertEqual(page.status_code, 200)
        self.assertIn(b'id="paired-move" class="solid-btn" disabled', page.data)
        self.assertFalse(owner.emit("move_relative", {"axis": "x", "mm": 5}, callback=True)["ok"])
        def command(client, action, token=None):
            return client.emit("paired_x", {"action": action, "token": token}, callback=True)
        self.assertFalse(command(owner, "move")["ok"])
        token = command(owner, "arm")["token"]
        self.assertNotIn(token, app.test_client().get("/api/status").get_data(as_text=True))
        self.assertFalse(command(other, "move", token)["ok"])
        other.disconnect()
        self.assertTrue(command(owner, "watch", token)["ok"])
        self.assertFalse(self.hw.request_scan()["ok"])
        self.assertTrue(command(owner, "move", token)["ok"])
        self.assertFalse(command(owner, "move", token)["ok"])
        self.assertTrue(self.bench.take_request())
        self.bench.execute(self.link)
        self.assertEqual(self.link.commands, ["PAIR?", "PAIR?", "ARM X"])
        self.assertEqual(self.link.bytes, [b"MOVE X\n", b"~"])
        result = self.bench.status()["result"]
        self.assertTrue(result["verified"])
        self.assertEqual((result["encoder"], result["index"]), (-3999, 1))
        self.assertEqual(set(result["drivers"]), {"X1", "X2"})
        self.assertFalse(command(owner, "arm")["ok"])
        owner.disconnect()

    def test_unavailable_firmware_stale_sample_absent_encoder_and_busy_scan_refuse_arm(self):
        self.link.capability = None
        self.bench.connect(self.link)
        self.assertFalse(self.hw.paired_request("arm", "owner", None)["ok"])
        self.link.capability = READY
        self.bench.connect(self.link)
        self.hw.received_tick -= 10
        self.assertFalse(self.hw.paired_request("arm", "owner", None)["ok"])
        self.hw.received_tick += 10
        self.hw.sample["encoder_x1"] = None
        self.assertFalse(self.hw.paired_request("arm", "owner", None)["ok"])
        self.hw.sample["encoder_x1"] = 0
        self.hw.sample["enables"][0] = 1
        self.assertFalse(self.hw.paired_request("arm", "owner", None)["ok"])
        self.hw.sample["enables"][0] = 0
        self.assertTrue(self.hw.request_scan()["ok"])
        self.assertFalse(self.hw.paired_request("arm", "owner", None)["ok"])
        self.assertEqual(self.link.bytes, [])

    def test_cancel_before_dispatch_and_during_arm_ack_never_issues_move(self):
        self.queue()
        self.bench.cancel("owner")
        self.bench.execute(self.link)
        self.assertNotIn("ARM X", self.link.commands)
        self.assertEqual(self.bench.status()["phase"], "idle")
        self.queue()
        self.link.after_arm = lambda: self.bench.cancel("owner")
        self.bench.execute(self.link)
        self.assertEqual(self.link.commands[-2:], ["ARM X", "STOP"])
        self.assertEqual(self.link.bytes, [])

    def test_owner_disconnect_in_motion_stops_and_records_partial_travel(self):
        self.queue()
        self.link.on_running = lambda: self.bench.cancel("owner")
        self.bench.execute(self.link)
        self.assertEqual(self.link.bytes.count(b"!"), 1)
        result = self.bench.status()["result"]
        self.assertEqual((result["run"], result["steps"], result["encoder"]), ("STOPPED", 17, -19))
        self.assertFalse(result["verified"])

    def test_silent_phone_stops_and_expired_arm_never_dispatches(self):
        with patch("hardware.paired_x.time.monotonic", return_value=time.monotonic()) as clock:
            self.queue()
            self.link.on_running = lambda: setattr(clock, "return_value", clock.return_value + 2.1)
            self.bench.execute(self.link)
            self.assertIn(b"!", self.link.bytes)
            self.bench.connect(self.link)
            token = self.bench.request("arm", "owner", None, True)["token"]
            for _ in range(30):
                clock.return_value += 1
                self.bench.request("watch", "owner", token, True)
            self.assertFalse(self.bench.request("move", "owner", token, True)["ok"])

    def test_lost_start_ack_reads_persisted_result_on_reconnect_without_replay(self):
        self.queue()
        self.link.lose_ack = True
        with self.assertRaises(TimeoutError) as error:
            self.bench.execute(self.link)
        self.hw._failed(error.exception)
        self.assertEqual(self.bench.status()["phase"], "unknown")
        self.link.capability = USED
        self.link.saved = STOPPED
        self.bench.connect(self.link)
        self.assertEqual(self.link.commands[-2:], ["PAIR?", "RESULT"])
        self.assertEqual(self.link.bytes.count(b"MOVE X\n"), 1)
        self.assertEqual(self.bench.status()["result"]["run"], "STOPPED")
        self.assertFalse(self.hw.paired_request("arm", "owner", None)["ok"])

    def test_uptime_restart_discards_an_arm_and_requires_new_capability(self):
        self.bench.observe(self.hw.sample)
        token = self.hw.paired_request("arm", "owner", None)["token"]
        self.assertFalse(self.bench.observe({**self.hw.sample, "uptime_ms": 5}))
        self.assertFalse(self.bench.request("move", "owner", token, True)["ok"])
        self.assertTrue(self.bench.needs_probe)
        self.assertEqual(self.link.bytes, [])

    def test_decoder_requires_encoder_index_and_ordered_healthy_driver_reports(self):
        rows = [report("X1"), report("X2")]
        for line in (DONE.replace("z_delta=1", "z_delta=0"), DONE.replace("-3999", "-8"),
                     DONE.replace("steps=3200", "steps=3199")):
            self.assertFalse(decode_result(line, iter(rows))["verified"])
        for wrong in (rows[1].replace("cfg=1", "cfg=0"), rows[1].replace("diag2=0xC000", "diag2=0xC001"),
                      rows[1].replace("fault=0xC000", "fault=0xC080"), rows[1].replace("hal=0", "hal=3")):
            self.assertFalse(decode_result(DONE, iter([rows[0], wrong]))["verified"])
        for incomplete in ([rows[0]], [rows[0], rows[0]], rows[::-1]):
            with self.assertRaises(ValueError):
                decode_result(DONE, iter(incomplete))


if __name__ == "__main__":
    unittest.main()
