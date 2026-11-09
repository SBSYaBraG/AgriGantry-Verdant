"""
config.py - AgriGantry Web Interface Configuration

THIS IS THE ONE FILE YOU EDIT WHEN THE MACHINE CHANGES SIZE.

Everything dimensional in the whole web interface derives from the AXES dict
below: the bed map's aspect ratio, the millimetre <-> step conversions, the
axis labels shown in the UI, and which ALM (alarm) indicator lights appear.
Change a number here, restart the server, and the entire interface follows.

The travel figures below describe the usable motion shown by the official
FarmBot Genesis v1.8 digital twin. They are slightly smaller than its nominal
3 m track length and 1.5 m gantry width so the moving assemblies stay inside
their mechanical end clearances. When measured calibration data is available,
edit the `travel_mm` values here and nothing else.
"""

import os

# -------------------- AXIS GEOMETRY --------------------
#
# One entry per axis. Keys are lowercase and are used as the axis ID
# everywhere else in the codebase (URLs, socket events, DOM element IDs).
#
#   label        - human-readable name shown in the interface
#   travel_mm    - total usable travel, from home (0) to the far limit switch
#   steps_per_mm - stepper steps per millimetre; converts UI millimetres into
#                  the raw step counts the Arduino firmware actually expects
#   motors       - the physical motors driving this axis. The ALM panel renders
#                  one indicator light per motor listed here, so this list is
#                  what controls that part of the UI.
#
# GABRIEL BRANCH FOUR-MOTOR DRIVE SCHEME
# ---------------------------
# The simulation follows the same axis ownership as the current manual-
# calibration firmware:
#
#   X = the whole gantry rolling along the bed's LENGTH.
#       TWO motors, XL and XR, one on each rail plate. They must stay
#       synchronised or the gantry racks and jams across the rails.
#   Y = the carriage riding across the top beam, over the bed's WIDTH.
#       ONE motor.
#   Z = the vertical extension sliding up and down through that carriage,
#       carrying the tool head. ONE motor.
#
# This is also the stock FarmBot convention: X is the long track direction,
# Y is the cross-slide, and Z is vertical.
AXES = {
    "x": {
        "label": "X (bed length)",
        "travel_mm": 2800,
        "steps_per_mm": 5,
        "motors": ["XL", "XR"],
    },
    "y": {
        "label": "Y (bed width)",
        "travel_mm": 1300,
        "steps_per_mm": 5,
        "motors": ["Y"],
    },
    "z": {
        "label": "Z (tool height)",
        "travel_mm": 500,
        "steps_per_mm": 5,
        "motors": ["Z"],
    },
}

# -------------------- BED MAP ORIENTATION --------------------
#
# Which axis is which on the top-down bed map. The map always draws the bed's
# LENGTH horizontally, because a long-and-narrow rectangle uses the width of a
# monitor far better than a tall-and-narrow one does.
#
# This mapping exists so the minimap and labels use the same stock FarmBot
# convention as the firmware and the 3D model.
BED_MAP = {
    "length": "x",   # axis running along the bed's length  -> horizontal on the map
    "width":  "y",   # axis running across the bed's width   -> vertical on the map
    "height": "z",   # axis running up and down              -> the separate Z gauge
}

# -------------------- SIMULATION BEHAVIOUR --------------------

# How fast each simulated axis travels, in millimetres per second.
# These remain visual/test values until the physical machine's axis speeds are
# measured. Keeping them per-axis mirrors the real controller, whose X, Y, and
# Z mechanisms do not share one cruise speed.
SIM_SPEED_MM_S = {
    "x": 240.0,
    "y": 80.0,
    "z": 80.0,
}

# How many times per second the simulation recalculates positions.
# 20 Hz is smooth to the eye without burning CPU on a Raspberry Pi.
SIM_TICK_HZ = 20.0

# -------------------- SERVER --------------------

# How often the server pushes a state update to every connected browser.
# Deliberately lower than SIM_TICK_HZ: the simulation needs fine time steps to
# move smoothly, but the browser only needs enough updates to look live. The
# CSS transition on the bed map smooths the gaps between these snapshots.
# Twelve updates per second makes the large 3D gantry look materially smoother
# than the old 8 Hz stream while staying conservative for a Raspberry Pi 3.
STATUS_BROADCAST_HZ = 12.0

# Port the web interface listens on. Browsers on the WiFi reach the interface
# at http://<pi-ip-address>:5000
#
# Overridable via the FARMBOT_PORT environment variable, because macOS claims
# port 5000 for its AirPlay Receiver. That does not affect the Raspberry Pi,
# so 5000 stays the default; developing on a Mac just needs, for example:
#     FARMBOT_PORT=5050 python run.py
SERVER_PORT = int(os.environ.get("FARMBOT_PORT", 5000))

# Bind address. 0.0.0.0 means "accept connections on every network interface",
# which is what allows phones and laptops elsewhere on the WiFi to connect.
# Binding to 127.0.0.1 instead would restrict access to the Pi itself only.
SERVER_HOST = "0.0.0.0"


# -------------------- DERIVED HELPERS --------------------
#
# Small helpers so the rest of the codebase never repeats these conversions.
# Every one of them reads from AXES above, so they stay correct automatically
# when the machine is resized.


def axis_ids():
    """Return the list of axis IDs, e.g. ['x', 'y', 'z']."""
    return list(AXES.keys())


def max_steps(axis_id):
    """Total travel of an axis expressed in stepper steps."""
    axis = AXES[axis_id]
    return int(axis["travel_mm"] * axis["steps_per_mm"])


def mm_to_steps(axis_id, mm):
    """Convert a millimetre distance into stepper steps for a given axis."""
    return int(round(mm * AXES[axis_id]["steps_per_mm"]))


def steps_to_mm(axis_id, steps):
    """Convert stepper steps into millimetres for a given axis."""
    return steps / AXES[axis_id]["steps_per_mm"]


def all_motor_names():
    """
    Flat list of every motor across every axis, e.g. ['X','YL','YR','Z'].

    Used to build the ALM status dictionary and, on the frontend, the row of
    alarm indicator lights. Because it is generated from AXES, adding or
    removing a motor there automatically adds or removes its indicator light.
    """
    names = []
    for axis in AXES.values():
        names.extend(axis["motors"])
    return names


def client_config():
    """
    The subset of this configuration the browser needs, as a JSON-safe dict.

    Served by the /api/config endpoint. The frontend uses this to draw the bed
    map at the correct proportions and to label controls, which means the
    geometry is never duplicated in JavaScript - there is exactly one source
    of truth for it, and this is that file.
    """
    return {
        "axes": {
            axis_id: {
                "label": axis["label"],
                "travel_mm": axis["travel_mm"],
                "steps_per_mm": axis["steps_per_mm"],
                "max_steps": max_steps(axis_id),
                "motors": axis["motors"],
            }
            for axis_id, axis in AXES.items()
        },
        "motors": all_motor_names(),
        # Tells the frontend which axis to draw where on the bed map, so the
        # JavaScript never has to assume an orientation.
        "bed_map": BED_MAP,
    }
