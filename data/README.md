# AgriGantry measurement data

This directory holds versioned evidence from hardware testing. Keep a raw
capture separate from a processed export, and use filenames that retain the
test date, machine configuration, and capture label.

## Layout

| Path | Contents |
| --- | --- |
| `encoder/arduino_exports/` | CSV summaries created by `Firmware/scripts/export_encoder_readings.ps1`. |
| `encoder/scope_exports/` | Manually exported oscilloscope CSV files. Keep each one paired by timestamp/label with its Arduino summary. |
| `manual_calibration/` | Future Pi-generated JSONL events, Markdown summaries, and operator tape/reference observations. |

The PlatformIO `log2file` monitor filter continues to write fresh raw serial
captures to `Firmware/logs/`. The exporter reads the newest capture there and
writes the smaller, shareable encoder summary into this archive.

Do not state that a current iCL test has no physical drift based only on a
commanded-pulse count. Record the tape/reference observation separately. The
future encoder implementation will add measured feedback fields to the same
evidence trail.

