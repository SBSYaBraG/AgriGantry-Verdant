# AgriGantry

AgriGantry is a senior-design prototype of a three-axis, FarmBot-inspired
gantry system. This repository brings together the Arduino Mega and STM32G474 firmware,
motion and encoder experiments, the Pi-hosted dashboard, and the
CAD assets used to communicate the intended machine.

The active implementation work is on the `Gabriel` branch. It is deliberately
organized so that the firmware that operates the current iCL motor drivers,
the bench test for the replacement encoder motors, and the Pi service
cannot be confused or built together by accident.

## Start here

| If you want to... | Start here | What it covers |
| --- | --- | --- |
| Operate or modify the Arduino firmware | [Firmware guide](Firmware/README.md) | PlatformIO targets, safe startup, manual calibration, keyboard controls, and testing boundaries. |
| Work on the STM32G474 Rev1 controller | [STM32G474 firmware guide](Firmware/STM32G474/README.md) | CubeMX/CMake firmware, Pi communication and the encoder-verified X-axis bench test. |
| Manually calibrate the installed iCL gantry | [Manual-calibration guide](Firmware/lib/Motors_Manual/README.md) | Pin map, endpoint workflow, safety behavior, and the keyboard controller. |
| Test the replacement 17HS19 encoder motor | [Encoder bench-test guide](Firmware/lib/Motors_Enc/README.md) | Isolated TB6600, AM26LV32E, and encoder-measurement procedure. |
| Continue the Pi scheduler, logs, or dashboard tab | [Pi integration contract](Webapp-Pi/MANUAL_CALIBRATION_INTEGRATION.md) | The planned Pi/Mega/browser responsibilities and safe endurance-test contract. |
| Run the dashboard | [Dashboard README](Webapp-Pi/webapp/README.md) | Garden simulation, 3D view, live controller telemetry and supervised X-axis bench controls. |
| Find recorded measurements | [Data guide](data/README.md) | Encoder exports, serial captures, and the reserved manual-calibration archive. |
| See the chronological engineering record | [Changelog](Firmware/CHANGELOG.md) | Dated firmware and test changes. |

## Current project status

- The installed Arduino gantry uses **manual calibration with the iCL
  drivers**. On boot it intentionally leaves drivers disabled, step outputs
  low, and performs no automatic move or homing.
- Manual calibration establishes an estimated 0-100% travel range by counting
  Mega-generated pulses while an operator visually confirms both endpoints.
  It is useful for supervised repeatability testing, but it is not proof that
  the mechanics did not skip steps. Its position data exists only for the
  current powered Arduino session.
- On the **Rev. 1 control-board bench**, the Pi 3 programmed the STM32 through ST-LINK and returned all four drivers' SPI reports over COMM. A LAN phone command produced one supervised X1 shaft revolution. Its encoder measured −3,998 counts and one index for 3,200 STEP pulses. U5/X2 participated electrically with its motor socket empty. [Hardware evidence](Firmware/STM32G474/README.md#test-15-hardware-result)
- The dashboard serves both the physical bench and a garden simulator. Physical homing, loaded multi-axis motion, sustained-load thermal testing and sensor/tool integration remain future system tests.

The project controls real motors. Read the firmware guide before uploading any
PlatformIO environment, keep the mechanism clear, and do not run two programs
against the Mega serial port at once.

## Repository map

| Path | Purpose |
| --- | --- |
| `Firmware/` | Arduino Mega PlatformIO project: motion libraries, test utilities, live serial-monitor logs, and firmware changelog. |
| `Firmware/STM32G474/` | STM32G474 Rev1 CubeMX/CMake bring-up firmware; separate from the Mega project. |
| `Firmware/lib/Motors_ICL/` | Existing iCL driver control used by the normal program. |
| `Firmware/lib/Motors_Manual/` | Safety-first manual endpoint calibration for the installed iCL system. |
| `Firmware/lib/Motors_Enc/` | Isolated magnetic-encoder motor bench-test support. |
| `Webapp-Pi/` | Raspberry Pi integration documentation and the dashboard project. |
| `Webapp-Pi/webapp/` | Flask/Socket.IO dashboard, controller adapters, garden simulator and Three.js assets. |
| `data/` | Versioned measurement archives and the planned location for manual-calibration endurance evidence. |
| `Farmbot 3D Models/` and `models/` | Mechanical/CAD assets and reference models. |
| `AI/` | Project AI/vision exploration assets. |

## Arduino Mega firmware entry points

The PlatformIO source filters intentionally compile one entry point at a time.
Do not rely on the editor's active tab to select what gets uploaded.

| PlatformIO environment | Entry point | Use | Motion at boot |
| --- | --- | --- | --- |
| `megaatmega2560` | `Firmware/src/main.cpp` | Normal iCL program | Review its configuration before use; automatic startup homing is disabled. |
| `megaatmega2560-encoder-test` | `Firmware/src/encoder_test_main.cpp` | Standalone replacement-motor/encoder bench work | No gantry-calibration routine. |
| `megaatmega2560-manual-calibration` | `Firmware/src/manual_calibration_main.cpp` | Current supervised iCL endpoint calibration and percentage moves | Drivers disabled; no automatic motion or homing. |

## Development path

1. Use manual calibration to establish an estimated travel range and make
   supervised percentage moves with the current iCL motors.
2. Record repeatability observations using tape/reference marks. The planned
   Pi logging and scheduling layer will preserve these observations without
   claiming physical feedback that is not available yet.
3. Use the encoder-verified Rev. 1 X1 bench result as the single-shaft reference.
4. Integrate the encoder motors and DRV8461-based controller, then add
   validated encoder/stall feedback before enabling automatic physical homing
   or unattended operation.
5. Extend the validated Pi-to-STM32 dashboard path as additional physical operations pass bench and mechanical tests.

## Working conventions

- Give the active controller serial port one owner. The Pi service owns COMM during dashboard operation.
- Treat current manual-calibration positions as `commanded_estimate`, not
  measured physical position.
- Keep generated PlatformIO, Python virtual-environment, cache, and trained
  model artifacts out of Git. The top-level `.gitignore` documents these
  exclusions.
- Add a dated entry to [the firmware changelog](Firmware/CHANGELOG.md) for a
  change that affects hardware behavior, testing, or system design.

## Related documentation

- [Firmware guide](Firmware/README.md)
- [Manual-calibration Pi integration plan](Webapp-Pi/MANUAL_CALIBRATION_INTEGRATION.md)
- [Dashboard README](Webapp-Pi/webapp/README.md)
- [Data guide](data/README.md)
- [Firmware changelog](Firmware/CHANGELOG.md)

