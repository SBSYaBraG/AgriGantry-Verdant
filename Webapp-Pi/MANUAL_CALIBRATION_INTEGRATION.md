# AgriGantry manual-calibration Pi integration plan

> Status: planning contract only. This document deliberately starts no serial
> connection, schedules no motion, and changes no Arduino behavior. It is the
> shared implementation guide for the future Pi service and web-app tab.

## Purpose

The current iCL manual-calibration firmware can establish an **estimated**
axis range by counting Mega-generated pulses while an operator visually confirms
each endpoint. The Pi must preserve those test records, run requested movement
profiles one command at a time, and later expose the information through the
web app.

This is an endurance/repeatability aid, not automatic physical homing. Until
raw encoder feedback, stall detection, or limit switches are validated, neither
the Arduino nor the Pi can prove that a commanded pulse produced physical
motion.

## Authority boundaries

```text
Browser dashboard -> Pi manual-calibration service -> Mega serial protocol
                                                  -> motor pulse/safety logic
```

- The **Mega** remains the only component that creates step pulses, applies
  acceleration/deceleration, rejects illegal moves, and latches ALM/E-stop.
- The **Pi** owns wall-clock scheduling, log files, serial-session management,
  command sequencing, dashboard state, and long-term reports.
- The **browser** never opens the Mega serial port and never creates a motor
  trajectory itself. It asks the Pi service to perform a named operation.
- Exactly one Pi process owns the Mega serial port. The desktop keyboard
  controller and the Pi service cannot run at the same time.

## Current Mega protocol available to the Pi

The present manual firmware already accepts these commands:

| Purpose | Commands |
|---|---|
| Identify / inspect | `?`, `STATUS`, `ALM`, `HELP` |
| Calibrate | `CAL X`, `CAL Y`, `CAL Z`, `J<axis>-1`, `J<axis>1`, `CONFIRM`, `CANCEL` |
| Move a calibrated axis | `PX0`, `PX10`, `PX25`, `PX50`, `PX75`, `PX90`, `PX100` and equivalent `PY`/`PZ` |
| Return to midpoint | `HOME X`, `HOME Y`, `HOME Z`, `HOME ALL` |
| Stop safely | `STOP` for a controlled stop; `O`/`ESTOP` for immediate latched stop; `CLEAR` only after ALM is normal |

Important current semantics:

- `HOME <axis>` and `HOME ALL` return to stored **50% midpoints**. They do not
  find a physical endpoint.
- `PX0` and `PX100` are calculated targets from the current session's manual
  calibration. They are not verified physical limits.
- Calibration and position data are RAM-only. An Arduino power cycle makes all
  axes untrusted until manually calibrated again.

## Test profiles to implement later

### `supervised_full_range`

Run only when an operator has visually cleared the machine and can compare the
moving pointer/carriage with tape marks. It is never schedulable.

```text
Per axis: 0 -> 10 -> 25 -> 50 -> 75 -> 90 -> 100 -> 50 percent
Order:    X, then Y, then Z; one move completes before the next starts
```

The user interface must require an explicit area-clear confirmation immediately
before starting this profile.

### `safe_endurance`

For a later Pi-scheduled repeatability run. It intentionally stays away from
the estimated endpoints.

```text
X: 10 -> 90 -> 50 percent
Y: 10 -> 90 -> 50 percent
Z: 25 -> 75 -> 50 percent
```

It may be requested once or scheduled at 10/30/custom minute intervals after
an explicit arm action. It must default to disabled and must never resume after
a Pi restart, Mega restart, alarm, E-stop, serial timeout, or failed move.

## Planned Pi package layout

```text
Webapp-Pi/manual_calibration/
  protocol.py       # TODO: exclusive serial connection and line parser
  runner.py         # TODO: runs named profiles one target at a time
  scheduler.py      # TODO: monotonic timer; disabled until explicitly armed
  logger.py         # TODO: JSONL/CSV writer and daily Markdown summary
  service.py        # TODO: single facade imported by CLI and web app
  cli.py            # TODO: local run-once/start/stop/status commands
  README.md         # TODO: setup and safe operating procedure
```

`service.py` is the single interface the future web app should call. The
dashboard must not duplicate movement sequences, timers, or serial parsing.

## Required service behavior

1. Open and verify the Mega using `?`; require the expected firmware ID.
2. Query `STATUS` before every profile and require all selected axes to report
   `ready=YES` in the current firmware session.
3. Send exactly one percentage command, wait for its completion/error line,
   record it, then issue the next command.
4. Apply a computed timeout with a safety margin to every move. On timeout,
   protocol parse failure, ALM, E-stop, or unexpected stop: send `O` when the
   port is still available, mark the run failed, disarm the scheduler, and do
   not send another move.
5. Do not infer physical success from matching commanded counts. Every current
   record must use `position_confidence=commanded_estimate`.
6. Require a new explicit arm after any process restart, power cycle, fault,
   or completed scheduled run policy change.

## Logging contract

Write an append-only raw event log plus a small human-readable report:

```text
data/manual_calibration/YYYY-MM-DD_events.jsonl
data/manual_calibration/YYYY-MM-DD_summary.md
```

Each raw event should contain at least:

```text
timestamp_utc, monotonic_ms, run_id, cycle_number, profile, event_type,
axis, target_percent, target_steps, reported_start_steps,
reported_end_steps, duration_ms, result, stop_or_alarm_reason,
position_confidence, operator_tape_observation
```

At calibration completion, also record `travel_steps`, axis, first-end
direction, and an optional operator-entered tape/reference note. The Markdown
report should summarize completed/aborted cycles, calibration ranges, alarms,
and only the observations actually entered by a human. It must not claim that
no physical drift occurred without an external measurement.

## Future encoder and driver-feedback extension

When the new motors and DRV8461-based integrated controller expose validated
raw feedback, extend the same records with:

```text
encoder_start, encoder_end, encoder_delta, expected_encoder_delta,
position_error_steps, missed_steps, stall_detected, position_confidence=verified
```

At that point, the service may support a true automatic calibration/homing
operation that finds physical endpoints through validated feedback. Do not
enable that behavior for the present iCL manual-calibration configuration.

## Web-app contract for Lisdany

Create a `Manual Calibration & Endurance` tab backed only by the Pi service.
It should display:

- Axis readiness and current-session `travel_steps`.
- `Estimated / commanded pulses only` until encoder verification exists.
- Run state, profile, cycle number, last event, next due time, and alarm state.
- `Run supervised full-range test` with explicit confirmation.
- `Run safe endurance once`, schedule arm/stop controls, and log download/view.
- `Center all axes` labelled as a midpoint return, not physical homing.

### Suggested service operations

```text
get_manual_calibration_status()
run_profile("supervised_full_range")
run_profile("safe_endurance")
arm_safe_endurance(interval_minutes)
stop_safe_endurance()
list_run_logs()
```

The Pi can later host this service on the local network. The browser connects
to the Pi web server by a LAN address or mDNS hostname such as `farmbot.local`;
the serial link remains private between Pi and Mega now, or Pi and STM later.
