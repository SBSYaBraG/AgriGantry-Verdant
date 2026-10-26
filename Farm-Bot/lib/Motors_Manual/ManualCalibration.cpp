#include "ManualCalibration.h"

#include "ManualMotion.h"
#include "ManualSafety.h"

namespace {

enum class CalibrationStage : uint8_t {
  READY,
  SEEK_FIRST_END,
  FIRST_END_CANDIDATE,
  WAIT_FOR_REVERSAL,
  MEASURE_SECOND_END,
  SECOND_END_CANDIDATE,
  COMPLETE,
};

struct AxisCalibration {
  long travelSteps;
  int8_t towardMaximumDirection;
  bool rangeKnown;
  bool positionKnown;
};

AxisCalibration calibration[4] = {};
ManualAxis selectedAxis = ManualAxis::NONE;
CalibrationStage stage = CalibrationStage::READY;
int8_t firstSeekDirection = 0;
unsigned long stoppedAtMs = 0;

uint8_t axisIndex(ManualAxis axis) { return static_cast<uint8_t>(axis); }

bool isOngoingCalibration() {
  return stage == CalibrationStage::SEEK_FIRST_END ||
      stage == CalibrationStage::FIRST_END_CANDIDATE ||
      stage == CalibrationStage::WAIT_FOR_REVERSAL ||
      stage == CalibrationStage::MEASURE_SECOND_END ||
      stage == CalibrationStage::SECOND_END_CANDIDATE;
}

const __FlashStringHelper* stageName(CalibrationStage value) {
  switch (value) {
    case CalibrationStage::READY: return F("READY");
    case CalibrationStage::SEEK_FIRST_END: return F("SEEK_FIRST_END");
    case CalibrationStage::FIRST_END_CANDIDATE: return F("FIRST_END_CANDIDATE");
    case CalibrationStage::WAIT_FOR_REVERSAL: return F("WAIT_FOR_REVERSAL");
    case CalibrationStage::MEASURE_SECOND_END: return F("MEASURE_SECOND_END");
    case CalibrationStage::SECOND_END_CANDIDATE: return F("SECOND_END_CANDIDATE");
    case CalibrationStage::COMPLETE: return F("COMPLETE");
  }
  return F("UNKNOWN");
}

void resetIdleTimer() { stoppedAtMs = 0; }

bool commandAllowed(ManualAxis axis) {
  if (!manualIsAxis(axis)) {
    Serial.println(F("ERR: select X, Y, or Z"));
    return false;
  }
  if (manualSafetyHasFault()) {
    Serial.println(F("ERR: clear emergency stop / ALM before moving"));
    return false;
  }
  return true;
}

}  // namespace

void manualCalibrationBegin() {
  for (uint8_t i = static_cast<uint8_t>(ManualAxis::X); i <= static_cast<uint8_t>(ManualAxis::Z); ++i) {
    calibration[i] = {0, 1, false, false};
  }
}

bool manualCalibrationSelect(ManualAxis axis) {
  if (!commandAllowed(axis) || !manualMotionIsIdle()) return false;
  if (isOngoingCalibration()) {
    Serial.println(F("ERR: finish or cancel the active calibration first"));
    return false;
  }
  selectedAxis = axis;
  stage = CalibrationStage::READY;
  Serial.print(F("SELECTED_AXIS "));
  Serial.println(manualAxisName(axis));
  return true;
}

bool manualCalibrationStart(ManualAxis axis) {
  if (!manualCalibrationSelect(axis)) return false;
  AxisCalibration& data = calibration[axisIndex(axis)];
  data.rangeKnown = false;
  data.positionKnown = false;
  data.travelSteps = 0;
  data.towardMaximumDirection = 1;
  manualMotionSetPosition(axis, 0);
  firstSeekDirection = 0;
  resetIdleTimer();
  stage = CalibrationStage::SEEK_FIRST_END;
  Serial.print(F("CALIBRATION_BEGIN "));
  Serial.println(manualAxisName(axis));
  Serial.println(F("Jog toward either endpoint, release, wait 2 seconds, then press Enter."));
  return true;
}

bool manualCalibrationJog(ManualAxis axis, int8_t direction) {
  if (!commandAllowed(axis) || axis != selectedAxis || direction < -1 || direction > 1) return false;

  if (direction == 0) {
    if (stage == CalibrationStage::SEEK_FIRST_END || stage == CalibrationStage::MEASURE_SECOND_END ||
        stage == CalibrationStage::FIRST_END_CANDIDATE || stage == CalibrationStage::SECOND_END_CANDIDATE) {
      return manualMotionRequestJog(axis, 0);
    }
    return false;
  }

  if (stage == CalibrationStage::SEEK_FIRST_END) {
    if (firstSeekDirection == 0) firstSeekDirection = direction;
    if (direction != firstSeekDirection) {
      Serial.println(F("ERR: reach and confirm the first endpoint before reversing"));
      return false;
    }
    resetIdleTimer();
    return manualMotionRequestJog(axis, direction);
  }

  if (stage == CalibrationStage::FIRST_END_CANDIDATE) {
    if (direction != firstSeekDirection) {
      Serial.println(F("ERR: confirm the first endpoint before reversing"));
      return false;
    }
    stage = CalibrationStage::SEEK_FIRST_END;
    resetIdleTimer();
    Serial.println(F("FIRST_ENDPOINT_CONFIRMATION_CANCELLED"));
    return manualMotionRequestJog(axis, direction);
  }

  if (stage == CalibrationStage::WAIT_FOR_REVERSAL) {
    if (direction != -firstSeekDirection) {
      Serial.println(F("ERR: press the opposite direction to capture 0%"));
      return false;
    }
    manualMotionSetPosition(axis, 0);
    if (!manualMotionRequestJog(axis, direction)) return false;
    calibration[axisIndex(axis)].towardMaximumDirection = direction;
    stage = CalibrationStage::MEASURE_SECOND_END;
    resetIdleTimer();
    Serial.println(F("ZERO_CAPTURED: counting pulses toward 100%"));
    return true;
  }

  if (stage == CalibrationStage::MEASURE_SECOND_END) {
    if (direction != -firstSeekDirection) {
      Serial.println(F("ERR: continue toward the second endpoint; do not reverse"));
      return false;
    }
    resetIdleTimer();
    return manualMotionRequestJog(axis, direction);
  }

  if (stage == CalibrationStage::SECOND_END_CANDIDATE) {
    if (direction != -firstSeekDirection) {
      Serial.println(F("ERR: only continue toward the second endpoint or confirm it"));
      return false;
    }
    stage = CalibrationStage::MEASURE_SECOND_END;
    resetIdleTimer();
    Serial.println(F("SECOND_ENDPOINT_CONFIRMATION_CANCELLED"));
    return manualMotionRequestJog(axis, direction);
  }

  Serial.println(F("ERR: jog commands are only allowed during manual calibration"));
  return false;
}

void manualCalibrationUpdate() {
  if (manualSafetyHasFault() || !isOngoingCalibration()) return;
  if (stage != CalibrationStage::SEEK_FIRST_END && stage != CalibrationStage::MEASURE_SECOND_END) return;
  if (!manualMotionIsIdle()) {
    resetIdleTimer();
    return;
  }
  if (stage == CalibrationStage::SEEK_FIRST_END && firstSeekDirection == 0) return;

  if (stoppedAtMs == 0) {
    stoppedAtMs = millis();
    return;
  }
  if (millis() - stoppedAtMs < MANUAL_ENDPOINT_IDLE_MS) return;

  if (stage == CalibrationStage::SEEK_FIRST_END) {
    stage = CalibrationStage::FIRST_END_CANDIDATE;
    Serial.println(F("FIRST_ENDPOINT_CANDIDATE: press Enter to confirm; Backspace to continue."));
  } else {
    stage = CalibrationStage::SECOND_END_CANDIDATE;
    Serial.println(F("SECOND_ENDPOINT_CANDIDATE: press Enter to confirm; Backspace to continue."));
  }
}

bool manualCalibrationConfirmEndpoint() {
  if (manualSafetyHasFault() || !manualMotionIsIdle()) return false;
  if (stage == CalibrationStage::FIRST_END_CANDIDATE) {
    stage = CalibrationStage::WAIT_FOR_REVERSAL;
    resetIdleTimer();
    Serial.println(F("FIRST_ENDPOINT_CONFIRMED: press the opposite jog direction to set 0%."));
    return true;
  }
  if (stage != CalibrationStage::SECOND_END_CANDIDATE) {
    Serial.println(F("ERR: no endpoint confirmation is pending"));
    return false;
  }

  AxisCalibration& data = calibration[axisIndex(selectedAxis)];
  const long rawPosition = manualMotionGetPosition(selectedAxis);
  const long measuredTravel = rawPosition * data.towardMaximumDirection;
  if (measuredTravel < MANUAL_MIN_CALIBRATION_STEPS) {
    Serial.println(F("ERR: measured travel is too short; continue jogging or restart calibration"));
    stage = CalibrationStage::MEASURE_SECOND_END;
    resetIdleTimer();
    return false;
  }
  data.travelSteps = measuredTravel;
  data.rangeKnown = true;
  data.positionKnown = true;
  manualMotionSetPosition(selectedAxis, data.towardMaximumDirection * measuredTravel);
  stage = CalibrationStage::COMPLETE;
  resetIdleTimer();
  Serial.print(F("CALIBRATION_COMPLETE "));
  Serial.print(manualAxisName(selectedAxis));
  Serial.print(F(" travel_steps="));
  Serial.println(measuredTravel);
  Serial.println(F("Use HOME <axis> or P<axis>10/25/50/75/90. Jogging is now locked."));
  // TODO: persist only the confirmed travel length with a version/CRC. Never
  // trust the current position after power loss without a new reference/home.
  return true;
}

bool manualCalibrationCancelEndpoint() {
  if (stage == CalibrationStage::FIRST_END_CANDIDATE) {
    stage = CalibrationStage::SEEK_FIRST_END;
    resetIdleTimer();
    Serial.println(F("FIRST_ENDPOINT_CONFIRMATION_CANCELLED"));
    return true;
  }
  if (stage == CalibrationStage::SECOND_END_CANDIDATE) {
    stage = CalibrationStage::MEASURE_SECOND_END;
    resetIdleTimer();
    Serial.println(F("SECOND_ENDPOINT_CONFIRMATION_CANCELLED"));
    return true;
  }
  Serial.println(F("ERR: no endpoint confirmation is pending"));
  return false;
}

bool manualCalibrationMovePercent(ManualAxis axis, uint8_t percent) {
  if (!commandAllowed(axis) || !manualMotionIsIdle() || isOngoingCalibration()) return false;
  if (percent != 0 && percent != 10 && percent != 25 && percent != 50 &&
      percent != 75 && percent != 90 && percent != 100) {
    Serial.println(F("ERR: use 0, 10, 25, 50, 75, 90, or 100 percent"));
    return false;
  }
  AxisCalibration& data = calibration[axisIndex(axis)];
  if (!data.rangeKnown || !data.positionKnown) {
    Serial.println(F("ERR: axis has no trusted manual calibration in this power session"));
    return false;
  }
  const long distance = static_cast<long>((static_cast<int64_t>(data.travelSteps) * percent + 50) / 100);
  const long target = distance * data.towardMaximumDirection;
  if (!manualMotionRequestTarget(axis, target)) return false;
  selectedAxis = axis;
  stage = CalibrationStage::COMPLETE;
  Serial.print(F("TARGET "));
  Serial.print(manualAxisName(axis));
  Serial.print(F(" percent="));
  Serial.println(percent);
  return true;
}

bool manualCalibrationHome(ManualAxis axis) {
  return manualCalibrationMovePercent(axis, 0);
}

ManualAxis manualCalibrationSelectedAxis() { return selectedAxis; }

bool manualCalibrationAxisReady(ManualAxis axis) {
  return manualIsAxis(axis) && calibration[axisIndex(axis)].rangeKnown &&
      calibration[axisIndex(axis)].positionKnown;
}

void manualCalibrationPrintStatus() {
  Serial.print(F("CALIBRATION stage="));
  Serial.print(stageName(stage));
  Serial.print(F(" | selected="));
  Serial.println(manualAxisName(selectedAxis));
  for (uint8_t i = static_cast<uint8_t>(ManualAxis::X); i <= static_cast<uint8_t>(ManualAxis::Z); ++i) {
    const ManualAxis axis = static_cast<ManualAxis>(i);
    const AxisCalibration& data = calibration[i];
    Serial.print(manualAxisName(axis));
    Serial.print(F(": travel="));
    Serial.print(data.travelSteps);
    Serial.print(F(" | raw_position="));
    Serial.print(manualMotionGetPosition(axis));
    Serial.print(F(" | ready="));
    Serial.println(data.rangeKnown && data.positionKnown ? F("YES") : F("no"));
  }
}
