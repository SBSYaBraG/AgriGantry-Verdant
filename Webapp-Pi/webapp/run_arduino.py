"""Host the dashboard with an Arduino Mega on USB."""

import argparse

from hardware.arduino import Arduino
from hardware.telemetry import SerialTelemetry
from run import main


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=Arduino.port)
    main(SerialTelemetry(Arduino(), parser.parse_args().port))
