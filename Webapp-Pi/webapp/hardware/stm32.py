"""AgriGantry Test 14 on the PCB COMM header (USART1, 115200 8N1)."""

import re


class STM32:
    mode = "stm32"
    port = "/dev/serial0"
    drivers = ("U4/X1", "U5/X2", "U6/Y", "U7/Z")
    status = re.compile(
        r"STATUS uptime_ms=(\d+) ENA_X=([01]) ENA_Y=([01]) ENA_Z=([01]) ENC_X1=(NA|-?\d+)"
    )
    driver_ok = re.compile(
        r"SPI=OK FAULT=(0x[0-9A-F]{2})/(0x[0-9A-F]{2}) nFAULT=([01]/[01]/[01]) "
        r"STATUS_BITS=(0x[0-9A-F]{2}) CTRL1=(0x[0-9A-F]{2}) CTRL2=(0x[0-9A-F]{2})"
    )

    def identify(self, link):
        if next(link.replies("PING")) != "PONG":
            raise ValueError("Expected PONG from the STM32.")
        return "STM32 USART1"

    def sample(self, link):
        reply = next(link.replies("STATUS"))
        match = self.status.fullmatch(reply)
        if not match:
            raise ValueError("Unexpected STM32 STATUS reply.")
        uptime, x, y, z, count = match.groups()
        return {"uptime_ms": int(uptime), "enables": [int(x), int(y), int(z)],
                "encoder_x1": None if count == "NA" else int(count)}

    def scan(self, link):
        """Test 11 wakes all drivers, reads registers, then returns them to sleep."""
        rows = {}
        for reply in link.replies("SCAN"):
            prefix, name, fields = reply.split(" ", 2)
            if prefix != "SCAN" or name not in self.drivers or name in rows:
                raise ValueError("Invalid or duplicate driver identity in SCAN.")
            match = self.driver_ok.fullmatch(fields)
            if match:
                first, settled, pins, bits, ctrl1, ctrl2 = match.groups()
                rows[name] = dict(spi="OK", first=first, settled=settled, pins=pins,
                                  status_bits=bits, ctrl1=ctrl1, ctrl2=ctrl2)
            elif re.fullmatch(r"SPI=FAIL FAULT=NA nFAULT=[01]/[01]/[01]", fields):
                rows[name] = dict(spi="FAIL", pins=fields.rsplit("=", 1)[1])
            elif fields == "SPI=NOT_RUN FAULT=NA nFAULT=NA":
                rows[name] = dict(spi="NOT_RUN")
            else:
                raise ValueError(f"Unexpected SCAN report from {name}.")
            if len(rows) == len(self.drivers):
                return rows
