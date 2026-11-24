"""Host the dashboard with the STM32 COMM UART."""

import argparse

from hardware.stm32 import STM32
from hardware.telemetry import SerialTelemetry
from run import main


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=STM32.port)
    parser.add_argument("--paired-x", action="store_true", help="Enable the supervised Test 15 bench controls")
    args = parser.parse_args()
    main(SerialTelemetry(STM32(), args.port, paired_x=args.paired_x))
