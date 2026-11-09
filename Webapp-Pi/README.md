# AgriGantry web app and Raspberry Pi

This directory contains the browser dashboard, the Raspberry Pi integration
plan, and a preserved legacy controller. The dashboard currently operates only
against simulated hardware; it does not command the physical AgriGantry.

## Start here

| Path | Purpose |
| --- | --- |
| `webapp/` | Flask/Socket.IO dashboard with optional Three.js visualization. |
| `MANUAL_CALIBRATION_INTEGRATION.md` | The agreed future contract for Pi serial ownership, safe schedules, logs, and dashboard controls. |
| `legacy_controller.py` | Earlier command-line prototype retained as reference only; not compatible with current manual calibration. |
| `README_LEGACY_CONTROLLER.md` | Historical command reference for that legacy prototype. |

## Current safety boundary

The browser dashboard must remain on `SimulatedHardware` until the planned Pi
manual-calibration service has been implemented and tested. When physical
integration begins, the Pi will be the only serial-port owner, the Mega will
remain the only step-pulse and motion-safety authority, and the browser will
request named service operations instead of talking to the Mega directly.

See [the integration plan](MANUAL_CALIBRATION_INTEGRATION.md) before adding a
serial backend, scheduler, or dashboard button that could affect hardware.

## Run the demonstration

Follow [webapp/README.md](webapp/README.md). Create the virtual environment
inside `webapp/.venv`; virtual environments are local, ignored, and should be
recreated after a folder rename rather than moved.

