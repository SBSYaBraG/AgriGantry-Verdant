# AgriGantry web app and Raspberry Pi

This directory contains the Pi-hosted browser dashboard, the integration plan for the Arduino gantry and a preserved legacy controller. A board-powered Pi 3 hosted the live Rev. 1 telemetry and supervised X-axis bench test. The garden simulator runs in its own profile.

## Start here

| Path | Purpose |
| --- | --- |
| `webapp/` | Flask/Socket.IO dashboard with optional Three.js visualization. |
| `MANUAL_CALIBRATION_INTEGRATION.md` | The agreed future contract for Pi serial ownership, safe schedules, logs, and dashboard controls. |
| `legacy_controller.py` | Earlier command-line prototype retained as reference only; not compatible with current manual calibration. |
| `README_LEGACY_CONTROLLER.md` | Historical command reference for that legacy prototype. |

## Dashboard profiles

The `stm32` profile reads COMM telemetry and offers a four-driver SPI scan. The `paired-x` profile adds one explicitly armed Test 15 move per STM32 boot. The controller generates the pulses and sends the measured result back through the Pi. The physical run used a phone on the LAN, a motor and encoder on X1, and an empty X2 motor socket. [Bench evidence](../Firmware/STM32G474/README.md#test-15-hardware-result)

The `arduino` profile reads Mega telemetry over USB. Future scheduling and calibration work for that platform has its own [integration plan](MANUAL_CALIBRATION_INTEGRATION.md). The `simulated` profile provides general jogging, homing, garden tasks and demo alerts.

## Run the demonstration

Follow [webapp/README.md](webapp/README.md). Create the virtual environment
inside `webapp/.venv`; virtual environments are local, ignored, and should be
recreated after a folder rename rather than moved.

