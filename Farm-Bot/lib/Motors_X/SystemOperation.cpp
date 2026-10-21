#include "SystemOperations.h"
#include "MotorControl.h"
#include "PositionManager.h"

void runHoming(Axis) {
  Serial.println(F("Homing unavailable: this gantry has no limit switches."));
  Serial.println(F("Move to a known origin, then use 0 or 0X / 0Y / 0Z."));
}
void runHomingAll() {
  runHoming(AXIS_NONE);
}
void zeroPosition(Axis axis) {
  if (anyAxisMoving()) {
    Serial.println(F("Cannot zero while moving; release keys and wait for STOPPED"));
    return;
  }
  if (isEmergencyStopActive()) {
    Serial.println(F("Clear the emergency stop before setting the position reference"));
    return;
  }
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    if (axis != AXIS_NONE && axis != (Axis)i) continue;
    setCurrentPosition((Axis)i, 0);
    setPositionReferenced((Axis)i, true);
  }
  Serial.println(F("Manual zero set. This records the current location; it does not move the gantry."));
}
