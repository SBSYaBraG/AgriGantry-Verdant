"""Arduino Mega USB telemetry for the Gabriel and Arturo presentation firmware."""

import re


class Arduino:
    mode = "arduino"
    port = "/dev/ttyACM0"
    manual = "AGRIGANTRY-MANUAL-CAL-v1"
    arturo = "AF434-GANTRY-v1"

    def identify(self, link):
        for line in link.replies("?"):
            if line in (self.manual, self.arturo):
                self.firmware = line
                return line

    def sample(self, link):
        axes, alarms = {}, {}
        estop = None
        active = None
        for line in link.replies("STATUS"):
            if self.firmware == self.arturo:
                axis = re.match(r"^([XYZ]): position=(-?\d+)\b", line)
                if axis:
                    percent = re.search(r"\bpercent=(\d{1,3})\b", line)
                    calibrated = re.search(r"\bmanual zero set\b", line, re.I) and percent is not None
                    pct = int(percent[1]) if calibrated else None
                    axes[axis[1].lower()] = dict(steps=int(axis[2]), percent=pct,
                                                 homed=pct is not None and 0 <= pct <= 100)
                if len(axes) == 3:
                    return {"axes": axes}
                continue

            motion = re.fullmatch(r"MOTION active=([-XYZ]) \| velocity=\d+ \| estop=(YES|no)", line)
            if motion:
                active, estop = motion[1], motion[2] == "YES"
            axis = re.fullmatch(r"([XYZ]): travel=(\d+) \| raw_position=(-?\d+) \| ready=(YES|no)", line)
            if axis:
                label, travel, raw, ready = axis.groups()
                travel, raw = int(travel), int(raw)
                known = ready == "YES" and 0 < travel and abs(raw) <= travel
                axes[label.lower()] = dict(steps=raw, homed=known, moving=label == active,
                                           percent=round(100 * abs(raw) / travel, 1) if known else None)
            fault = re.fullmatch(r"(X-left|X-right|Y|Z): (ALARM|OK) \| latched=(YES|no)", line)
            if fault:
                label = {"X-left": "XL", "X-right": "XR"}.get(fault[1], fault[1])
                alarms[label] = fault[2] == "ALARM" or fault[3] == "YES"
            if len(axes) == 3 and len(alarms) == 4 and estop is not None:
                return dict(axes=axes, alm=alarms, alm_critical=any(alarms.values()), emergency_stop=estop)
