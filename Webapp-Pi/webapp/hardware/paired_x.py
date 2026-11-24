"""Supervised one-revolution bench command. Only the serial worker writes to COMM."""

import re
import secrets
import threading
import time


RESULT = re.compile(r"RESULT X run=([A-Z0-9_]+) steps=(\d+) enc_delta=(NA|-?\d+) z_delta=(NA|\d+)"
                    r"(?: ab_range=(OK|OUT|NA) ms=(\d+))?")
DRIVER = re.compile(r"DRV (X[12]) cfg=([01]) ctrl1=(0x[0-9A-F]{2}) n=(\d+) fault=(0x[0-9A-F]{4})"
                    r" diag2=(0x[0-9A-F]{4}) reg=(0x[0-9A-F]{2}) hal=(\d+) raw=(0x[0-9A-F]{4})")


def decode_result(first, replies):
    match = RESULT.fullmatch(first)
    if not match:
        raise ValueError("Unexpected paired-X result.")
    run, steps, encoder, index, band, ms = match.groups()
    drivers = {}
    for name in ("X1", "X2"):
        row = DRIVER.fullmatch(next(replies, ""))
        if not row or row[1] != name:
            raise ValueError(f"Missing result for {name}.")
        drivers[name] = dict(configured=row[2] == "1", ctrl1=row[3], samples=int(row[4]), fault=row[5],
                             diag2=row[6], reg=row[7], hal=int(row[8]), raw=row[9])
    delta = None if encoder == "NA" else int(encoder)
    z = None if index == "NA" else int(index)
    driver_checks = all(d["configured"] and d["ctrl1"] == "0x8F" and d["samples"] > 0 and
                        d["fault"] == "0xC000" and (int(d["diag2"], 16) & 0xFF63) == 0xC000
                        and d["hal"] == 0 for d in drivers.values())
    verified = (run == "DONE" and int(steps) == 3200 and delta is not None and
                3800 <= abs(delta) <= 4200 and band == "OK" and z == 1 and driver_checks)
    return dict(run=run, steps=int(steps), encoder=delta, index=z, ms=None if ms is None else int(ms),
                drivers=drivers, verified=verified)


class PairedXBench:
    arm_seconds = 30
    operator_timeout = 2.0
    active = ("armed", "queued", "starting", "running", "stopping")

    def __init__(self):
        self.lock = threading.RLock()
        self.phase = "unavailable"
        self.supported = False
        self.owner = self.token = self.result = None
        self.arm_until = self.last_watch = 0
        self.uptime = None
        self.needs_probe = False
        self.detail = "Waiting for Test 15 firmware."

    def _expire(self):
        now = time.monotonic()
        if self.phase in ("armed", "queued") and (now >= self.arm_until or now - self.last_watch >= self.operator_timeout):
            self.cancel()
        elif self.phase in ("starting", "running") and now - self.last_watch >= self.operator_timeout:
            self.cancel()

    def busy(self):
        with self.lock:
            self._expire()
            return self.phase in self.active

    def status(self):
        with self.lock:
            self._expire()
            return dict(supported=self.supported, phase=self.phase, detail=self.detail, result=self.result)

    def cancel(self, sid=None):
        with self.lock:
            if sid is not None and self.owner != sid:
                return
            if self.phase in ("armed", "queued"):
                self.phase, self.detail = "idle", "Arm cancelled. Both drivers remain disabled."
                self.owner = self.token = None
            elif self.phase in ("starting", "running"):
                self.phase, self.detail = "stopping", "Stop requested. Waiting for the controller."

    def request(self, action, sid, token, ready):
        with self.lock:
            self._expire()
            if action == "stop":
                self.cancel()
                return {"ok": True}
            if action == "arm" and self.phase == "idle" and self.supported and ready:
                self.owner, self.token = sid, secrets.token_urlsafe(16)
                self.last_watch = time.monotonic()
                self.arm_until = self.last_watch + self.arm_seconds
                self.phase, self.detail = "armed", "Armed on the phone for 30 seconds. Outputs are disabled."
                return {"ok": True, "token": self.token}
            if self.owner != sid or self.token is None or self.token != token:
                return {"ok": False, "error": "Arm the test from this phone first."}
            if action == "watch" and self.phase in self.active:
                self.last_watch = time.monotonic()
                return {"ok": True}
            if action == "move" and self.phase == "armed" and ready:
                self.phase, self.detail = "queued", "Checking the STM32 before motion."
                return {"ok": True}
            return {"ok": False, "error": "This command is unavailable in the current bench state."}

    def link_failed(self, reason):
        with self.lock:
            if self.phase in ("starting", "running", "stopping", "unknown"):
                self.phase, self.detail = "unknown", "Result unconfirmed. Reconnecting to read the STM32 result."
            else:
                self.phase, self.detail = "unavailable", reason
            self.owner = self.token = None
            self.supported = False

    def connect(self, link):
        with self.lock:
            self.needs_probe = False
        try:
            state = next(link.replies("PAIR?"))
        except TimeoutError:  # Test 14 still supports the full read-only dashboard.
            state = ""
        if state not in ("PAIR X v=1 state=READY", "PAIR X v=1 state=USED"):
            self.link_failed("Test 15 firmware is required for paired motion.")
            return
        with self.lock:
            self.supported = True
            self.owner = self.token = None
            if state.endswith("=READY"):
                self.phase, self.detail = "idle", "Ready for one supervised shaft revolution."
                self.result = None
                return
            self.phase, self.detail = "used", "The one move for this boot has been used."
        # RESULT is observational. Recover an interrupted reply without issuing another move.
        replies = link.replies("RESULT")
        first = next(replies)
        if first.startswith("RESULT X1 "):
            if "run=NONE" not in first:
                next(replies)  # Consume the separate legacy DRV line.
            with self.lock:
                self.detail = "The X1-only test used this boot's move."
                self.result = None
            return
        self._finish(decode_result(first, replies))

    def observe(self, sample):
        with self.lock:
            if self.uptime is not None and sample["uptime_ms"] < self.uptime:
                self.cancel()
                self.needs_probe = True
                self.phase, self.supported = "unavailable", False
            self.uptime = sample["uptime_ms"]
            ready = sample.get("enables") == [0, 0, 0] and sample.get("encoder_x1") is not None
            if not ready:
                self.cancel()
            return ready and not self.needs_probe

    def take_request(self):
        with self.lock:
            self._expire()
            if self.phase != "queued":
                return False
            self.phase = "starting"
            return True

    def _finish(self, result):
        with self.lock:
            self.result = result
            self.phase = "finished"
            self.owner = self.token = None
            self.detail = ("One revolution verified by the X1 encoder." if result["verified"] else
                           "Review the captured result. Both drivers have been commanded off.")

    def execute(self, link):
        if next(link.replies("PAIR?")) != "PAIR X v=1 state=READY":
            raise ValueError("The STM32 refused a fresh paired-X attempt.")
        with self.lock:
            self._expire()
            if self.phase != "starting":
                self.phase, self.detail = "idle", "Arm cancelled before motion."
                self.owner = self.token = None
                return
        if next(link.replies("ARM X")) != "ARMED X window_ms=30000":
            raise ValueError("The STM32 did not acknowledge ARM X.")

        started = stop_sent = False
        next_beat = 0

        def service():
            nonlocal stop_sent, next_beat
            with self.lock:
                self._expire()
                if self.phase == "stopping" and not stop_sent:
                    link.write(b"!")  # Single byte for the non-FIFO STM32 receiver.
                    stop_sent = True
                elif started and not stop_sent and time.monotonic() >= next_beat:
                    link.write(b"~")
                    next_beat = time.monotonic() + 0.2

        try:
            with self.lock:
                self._expire()
                cancelled = self.phase != "starting"
                if not cancelled:
                    # A write with an uncertain outcome must never be retried.
                    link.write(b"MOVE X\n")
            if cancelled:
                next(link.replies("STOP"))
                with self.lock:
                    self.phase, self.detail = "idle", "Arm cancelled before motion."
                    self.owner = self.token = None
                return
            replies = link.lines(timeout=40, service=service)
            if next(replies) != "MOVE X START":
                raise ValueError("Missing MOVE X START acknowledgement.")
            started = True
            with self.lock:
                if self.phase == "starting":
                    self.phase, self.detail = "running", "X is running. Keep this page open."
            first = next(replies)
            started = False  # Firmware drops the common X enable and sleep before RESULT.
            self._finish(decode_result(first, replies))
        except (OSError, ValueError, StopIteration):
            if not stop_sent:
                try:
                    link.write(b"!")
                except OSError:
                    pass  # The STM32 also stops on a missing heartbeat.
            raise
