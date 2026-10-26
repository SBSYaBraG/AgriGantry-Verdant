#include "ManualSafety.h"

#include "ManualMotion.h"

namespace {

struct AlarmState {
  bool signal;
  bool lastSignal;
  bool latched;
  unsigned long changedAt;
};

AlarmState alarms[MANUAL_MOTOR_COUNT] = {};
bool faultLatched = false;
const char* const motorNames[MANUAL_MOTOR_COUNT] = {"X-left", "X-right", "Y", "Z"};

bool anyAlarmSignalHigh() {
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    if (digitalRead(MANUAL_MOTOR_PINS[i].alm) == MANUAL_ALM_ACTIVE_LEVEL) return true;
  }
  return false;
}

}  // namespace

void manualSafetyBegin() {
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    // Arturo's controller uses HIGH=fault, LOW=normal. INPUT_PULLUP makes an
    // unplugged ALM line fail safe instead of silently allowing motion.
    pinMode(MANUAL_MOTOR_PINS[i].alm, INPUT_PULLUP);
    const bool signal = digitalRead(MANUAL_MOTOR_PINS[i].alm);
    alarms[i] = {signal, signal, false, millis()};
  }
}

void manualSafetyPoll() {
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    AlarmState& alarm = alarms[i];
    const bool signal = digitalRead(MANUAL_MOTOR_PINS[i].alm);
    if (signal != alarm.lastSignal) {
      alarm.lastSignal = signal;
      alarm.changedAt = millis();
    }
    alarm.signal = signal;
    if (!alarm.latched && signal == MANUAL_ALM_ACTIVE_LEVEL &&
        millis() - alarm.changedAt >= MANUAL_ALM_DEBOUNCE_MS) {
      alarm.latched = true;
      faultLatched = true;
      manualMotionEmergencyStop();
      Serial.print(F("ALARM_LATCHED "));
      Serial.println(motorNames[i]);
    }
  }
}

bool manualSafetyHasFault() { return faultLatched || manualMotionEmergencyActive(); }

void manualSafetyEmergencyStop(const __FlashStringHelper* reason) {
  const bool wasLatched = faultLatched || manualMotionEmergencyActive();
  faultLatched = true;
  manualMotionEmergencyStop();
  if (!wasLatched) {
    Serial.print(F("EMERGENCY_STOP "));
    Serial.println(reason);
  }
}

bool manualSafetyClearFault() {
  manualSafetyPoll();
  if (anyAlarmSignalHigh()) {
    Serial.println(F("CLEAR_REJECTED: ALM input is still HIGH"));
    return false;
  }
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    alarms[i].latched = false;
    alarms[i].signal = false;
    alarms[i].lastSignal = false;
    alarms[i].changedAt = millis();
  }
  faultLatched = false;
  manualMotionClearEmergency();
  Serial.println(F("EMERGENCY_CLEARED: drivers remain disabled"));
  return true;
}

void manualSafetyPrintStatus() {
  Serial.println(F("ALM_STATUS HIGH=fault LOW=normal"));
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    Serial.print(motorNames[i]);
    Serial.print(F(": "));
    Serial.print(digitalRead(MANUAL_MOTOR_PINS[i].alm) == MANUAL_ALM_ACTIVE_LEVEL ? F("ALARM") : F("OK"));
    Serial.print(F(" | latched="));
    Serial.println(alarms[i].latched ? F("YES") : F("no"));
  }
  // TODO(DRV8461): When the new controller exposes raw A/B/Z feedback to the
  // Mega through proper differential receivers, compare commanded pulses and
  // encoder ticks here. Do not treat this coarse ICL ALM signal as encoder
  // feedback; verify its exact stall/fault timing before any auto-homing use.
}
