"""One serial owner per dashboard. Browser requests never write directly to a port."""

from datetime import datetime, timezone
import os
import shutil
import subprocess
import threading
import time

import serial

from hardware.paired_x import PairedXBench


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


class LineLink:
    def __init__(self, port):
        self.port = port

    def write(self, data):
        if self.port.write(data) != len(data):
            raise OSError("Incomplete serial write.")

    def replies(self, command, timeout=2, service=None):
        self.write((command + "\n").encode("ascii"))
        yield from self.lines(timeout, service)

    def lines(self, timeout, service=None):
        deadline = time.monotonic() + timeout
        buf = bytearray()
        while time.monotonic() < deadline:
            if service:
                service()
            buf.extend(self.port.read_until(b"\n", 161 - len(buf)))
            if len(buf) > 160:
                raise ValueError("Controller reply exceeded the line limit.")
            if buf.endswith(b"\n"):
                line = buf.decode("ascii").rstrip("\r\n")
                buf.clear()
                if line:
                    yield line
        raise TimeoutError(f"Controller response timed out after {timeout:g} seconds.")


class SerialTelemetry:
    commands_enabled = False  # Generic jog and homing commands belong to the simulator.
    poll_interval = 1.0
    stale_after = 3.0

    def __init__(self, protocol, port=None, *, paired_x=False):
        self.protocol = protocol
        self.mode = protocol.mode
        self.device = port or protocol.port
        self.lock = threading.Lock()
        self.bench = PairedXBench() if paired_x and self.mode == "stm32" else None
        self.shutdown = threading.Event()
        self.scan_pending = False
        self.scan_result = None
        self.sample = {}
        self.sequence = 0
        self.received_at = self.received_tick = None
        self.identity = None
        self.error = "Waiting for the controller."
        self.thread = None

    def start(self):
        self.thread = threading.Thread(target=self._run, name="controller-telemetry", daemon=True)
        self.thread.start()

    def stop(self):
        if self.bench:
            self.bench.cancel()
        self.shutdown.set()
        if self.thread:
            self.thread.join(timeout=5)

    def drain_log(self):
        return []

    def _fresh(self):
        return not self.error and self.received_tick is not None and time.monotonic() - self.received_tick < self.stale_after

    def get_status(self):
        with self.lock:
            return {
                "mode": self.mode, "commands_enabled": False, "hardware_connected": self._fresh(),
                "controller": self.identity, "port": self.device, "error": self.error,
                "sample_time": self.received_at, "sample_number": self.sequence,
                "axes": {}, "alm": {}, "emergency_stop": None, "alm_critical": None,
                "scan_pending": self.scan_pending, "scan": self.scan_result, **self.sample,
                "paired_x": self.bench.status() if self.bench else None,
            }

    def request_scan(self):
        with self.lock:
            if self.mode != "stm32" or not self._fresh() or self.sample.get("enables") != [0, 0, 0]:
                return {"ok": False, "error": "The scan needs a fresh, idle STM32 status."}
            if self.scan_pending:
                return {"ok": False, "error": "A driver scan is already pending."}
            if self.bench and self.bench.busy():
                return {"ok": False, "error": "Finish or cancel the X test first."}
            self.scan_pending = True
            return {"ok": True}

    def paired_request(self, action, sid, token):
        if not self.bench:
            return {"ok": False, "error": "Select the paired-X bench profile for this test."}
        with self.lock:
            ready = (self._fresh() and not self.scan_pending and self.sample.get("enables") == [0, 0, 0]
                     and self.sample.get("encoder_x1") is not None)
            return self.bench.request(action, sid, token, ready)

    def _open(self):
        # fuser also detects terminals that open the UART without an advisory lock.
        if os.name == "posix" and shutil.which("fuser"):
            owners = subprocess.run(["fuser", self.device], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if owners.returncode == 0:
                raise OSError(f"{self.device} is in use. Close the serial terminal first.")
        port = serial.Serial(baudrate=115200, timeout=0.12, write_timeout=0.5,
                             exclusive=True if os.name == "posix" else None)
        port.dtr = port.rts = False
        port.port = self.device
        port.open()
        return port

    def _failed(self, error):
        with self.lock:
            self.error = str(error)
            if self.bench:
                self.bench.link_failed(str(error))
            if self.scan_pending:
                self.scan_result = {"time": utc_now(), "error": str(error)}
                self.scan_pending = False

    def _run(self):
        while not self.shutdown.is_set():
            try:
                with self._open() as port:
                    if self.mode == "arduino" and self.shutdown.wait(2.5):
                        return  # USB opening can restart a Mega.
                    link = LineLink(port)
                    while not self.shutdown.is_set():
                        try:
                            port.reset_input_buffer()
                            identity = self.protocol.identify(link)
                            if self.bench:
                                self.bench.connect(link)
                            self._poll(link, identity)
                        except (ValueError, TimeoutError) as exc:
                            self._failed(exc)
                        if self.shutdown.wait(3):
                            return
            except (OSError, serial.SerialException) as exc:
                self._failed(exc)
            self.shutdown.wait(3)

    def _poll(self, link, identity):
        while not self.shutdown.is_set():
            sample = self.protocol.sample(link)
            with self.lock:
                self.sample = sample
                self.identity, self.error = identity, None
                self.received_at, self.received_tick = utc_now(), time.monotonic()
                self.sequence += 1
                scan = self.scan_pending
            if scan:
                if sample.get("enables") == [0, 0, 0]:
                    result = {"drivers": self.protocol.scan(link), "time": utc_now()}
                else:
                    result = {"error": "An enable was high at the pre-scan check.", "time": utc_now()}
                with self.lock:
                    self.scan_result, self.scan_pending = result, False
            elif self.bench:
                if self.bench.observe(sample) and self.bench.take_request():
                    self.bench.execute(link)
                if self.bench.needs_probe:
                    self.bench.connect(link)
            self.shutdown.wait(self.poll_interval)
