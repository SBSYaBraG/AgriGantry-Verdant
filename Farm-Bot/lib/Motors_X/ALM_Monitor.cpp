#include "ALM_Monitor.h"
#include "PositionManager.h"
#include "MotorControl.h"

static const char* motorNames[MOTOR_COUNT] = {"X-Left (M1)", "X-Right (M2)", "Y (M3)", "Z (M4)"};

void monitorAllALM() {
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) monitorMotorALM((Motor)i);
}
void monitorMotorALM(Motor motor) {
  if (motor < MOTOR_XL || motor >= MOTOR_COUNT) return;
  MotorALM& alm = getMotorALM(motor);
  unsigned long now = millis();
  alm.signal = digitalRead(MOTOR_PINS[motor].alm);
  if (alm.signal != alm.lastSignal) {
    alm.lastSignal = alm.signal;
    alm.signalChangedAt = now;
  }
  // AF-434's ALM output is active HIGH; any sustained fault latches a stop.
  if (!alm.critical && alm.signal == ALM_ACTIVE_LEVEL &&
      now - alm.signalChangedAt >= ALM_DEBOUNCE_MS) {
    alm.critical = true;
    emergencyStop();
    Serial.print(F("ALARM: "));
    Serial.println(getMotorName(motor));
  }
}
bool isAnyALMCritical() {
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    if (getMotorALM((Motor)i).critical) return true;
  }
  return false;
}
bool isMotorALMCritical(Motor motor) {
  return motor >= MOTOR_XL && motor < MOTOR_COUNT && getMotorALM(motor).critical;
}
void printALMStatus() {
  Serial.println(F("ALM: HIGH=fault, LOW=normal; four motors"));
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    Serial.print(motorNames[i]);
    Serial.print(digitalRead(MOTOR_PINS[i].alm) == ALM_ACTIVE_LEVEL ? F(": ALARM") : F(": OK"));
    Serial.println(getMotorALM((Motor)i).critical ? F(" | LATCHED") : F(" | clear"));
  }
}
const char* getMotorName(Motor motor) {
  return motor >= MOTOR_XL && motor < MOTOR_COUNT ? motorNames[motor] : "Unknown";
}
