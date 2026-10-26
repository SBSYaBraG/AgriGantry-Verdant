#include "ManualMotion.h"

#include <math.h>

#ifdef ARDUINO_ARCH_AVR
#include <avr/interrupt.h>
#include <util/atomic.h>
#define MANUAL_ATOMIC ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#else
#define MANUAL_ATOMIC for (bool manualOnce = true; manualOnce; manualOnce = false)
#endif

namespace {

enum class MotionMode : uint8_t { IDLE, JOG, TARGET };

volatile long positions[4] = {0, 0, 0, 0};
volatile uint16_t pulseRate = 0;
volatile uint16_t pulseAccumulator = 0;
volatile int8_t pulseDirection = 1;
volatile bool targetActive = false;
volatile long targetPosition = 0;
volatile bool targetReached = false;
bool targetCancelled = false;
bool lastTargetReached = false;
volatile bool emergencyActive = false;
volatile ManualAxis timerAxis = ManualAxis::NONE;

MotionMode mode = MotionMode::IDLE;
ManualAxis activeAxis = ManualAxis::NONE;
int8_t requestedDirection = 0;
int8_t configuredDirection = 1;
float currentSpeed = 0.0f;
float cruiseSpeed = 0.0f;
float acceleration = 0.0f;
float deceleration = 0.0f;
unsigned long lastControlUs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long enabledAtMs = 0;

uint8_t axisIndex(ManualAxis axis) {
  return static_cast<uint8_t>(axis);
}

void writePulsePins(ManualAxis axis, uint8_t level) {
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    if (MANUAL_MOTOR_PINS[i].axis == axis) {
      digitalWrite(MANUAL_MOTOR_PINS[i].pul, level);
    }
  }
}

void setDirection(ManualAxis axis, int8_t direction) {
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    const ManualMotorPins& motor = MANUAL_MOTOR_PINS[i];
    if (motor.axis != axis) continue;
    const bool high = direction > 0 ? motor.positiveDirectionHigh : !motor.positiveDirectionHigh;
    digitalWrite(motor.dir, high ? HIGH : LOW);
  }
  configuredDirection = direction;
  pulseDirection = direction;
}

void enableAxis(ManualAxis axis) {
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    if (MANUAL_MOTOR_PINS[i].axis == axis) {
      digitalWrite(MANUAL_MOTOR_PINS[i].ena, MANUAL_ENA_ENABLED_LEVEL);
    }
  }
  enabledAtMs = millis();
}

void disableAxis(ManualAxis axis) {
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    if (MANUAL_MOTOR_PINS[i].axis == axis) {
      digitalWrite(MANUAL_MOTOR_PINS[i].ena, MANUAL_ENA_DISABLED_LEVEL);
    }
  }
}

void disableAllAxes() {
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    digitalWrite(MANUAL_MOTOR_PINS[i].pul, LOW);
    digitalWrite(MANUAL_MOTOR_PINS[i].ena, MANUAL_ENA_DISABLED_LEVEL);
  }
}

void setPulseRate(float speed) {
  const uint16_t nextRate = speed <= 0.5f ? 0 : static_cast<uint16_t>(speed + 0.5f);
  MANUAL_ATOMIC { pulseRate = nextRate; }
}

void finishMotion(bool reachedTarget) {
  const ManualAxis finishedAxis = activeAxis;
  setPulseRate(0.0f);
  targetActive = false;
  targetReached = false;
  lastTargetReached = reachedTarget;
  currentSpeed = 0.0f;
  requestedDirection = 0;
  mode = MotionMode::IDLE;
  activeAxis = ManualAxis::NONE;
  timerAxis = ManualAxis::NONE;
  if (manualIsAxis(finishedAxis)) disableAxis(finishedAxis);

  Serial.print(reachedTarget ? F("MOVE_COMPLETE ") : F("STOPPED "));
  Serial.println(manualAxisName(finishedAxis));
}

bool startMotion(ManualAxis axis, MotionMode nextMode, int8_t direction, long target) {
  if (!manualIsAxis(axis) || direction < -1 || direction > 1 || direction == 0 || emergencyActive) {
    return false;
  }
  if (mode != MotionMode::IDLE) return false;

  activeAxis = axis;
  timerAxis = axis;
  mode = nextMode;
  requestedDirection = direction;
  currentSpeed = 0.0f;
  pulseAccumulator = 0;
  targetPosition = target;
  targetReached = false;
  targetCancelled = false;
  lastTargetReached = false;
  targetActive = nextMode == MotionMode::TARGET;
  cruiseSpeed = nextMode == MotionMode::JOG ? manualCalibrationCruisePps(axis) : manualNormalCruisePps(axis);
  acceleration = nextMode == MotionMode::JOG ? MANUAL_CALIBRATION_ACCEL_PPS2 : MANUAL_NORMAL_ACCEL_PPS2;
  deceleration = nextMode == MotionMode::JOG ? MANUAL_CALIBRATION_DECEL_PPS2 : MANUAL_NORMAL_DECEL_PPS2;
  setDirection(axis, direction);
  writePulsePins(axis, LOW);
  enableAxis(axis);
  lastHeartbeatMs = millis();
  lastControlUs = micros();
  return true;
}

}  // namespace

#ifdef ARDUINO_ARCH_AVR
ISR(TIMER1_COMPA_vect) { manualMotionTick(); }
#endif

void manualMotionBegin() {
#ifdef ARDUINO_ARCH_AVR
  TIMSK1 = 0;
#endif
  for (uint8_t i = 0; i < MANUAL_MOTOR_COUNT; ++i) {
    const ManualMotorPins& motor = MANUAL_MOTOR_PINS[i];
    digitalWrite(motor.pul, LOW);
    pinMode(motor.pul, OUTPUT);
    digitalWrite(motor.dir, LOW);
    pinMode(motor.dir, OUTPUT);
    digitalWrite(motor.ena, MANUAL_ENA_DISABLED_LEVEL);
    pinMode(motor.ena, OUTPUT);
  }
  disableAllAxes();
  lastControlUs = micros();

#ifdef ARDUINO_ARCH_AVR
  MANUAL_ATOMIC {
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = static_cast<uint16_t>(F_CPU / MANUAL_STEP_TICK_HZ) - 1;
    TCCR1B = (1 << WGM12) | (1 << CS10);
    TIMSK1 = (1 << OCIE1A);
  }
#endif
}

void manualMotionTick() {
  if (emergencyActive || !manualIsAxis(timerAxis)) return;
  const uint16_t rate = pulseRate;
  if (rate == 0) return;

  uint16_t accumulator = pulseAccumulator + rate;
  if (accumulator < MANUAL_STEP_TICK_HZ) {
    pulseAccumulator = accumulator;
    return;
  }
  pulseAccumulator = accumulator - MANUAL_STEP_TICK_HZ;

  const ManualAxis axis = timerAxis;
  const uint8_t index = axisIndex(axis);
  const long position = positions[index];
  if (targetActive && (pulseDirection > 0 ? position >= targetPosition : position <= targetPosition)) {
    pulseRate = 0;
    targetReached = true;
    return;
  }

  writePulsePins(axis, HIGH);
  delayMicroseconds(MANUAL_STEP_PULSE_US);
  writePulsePins(axis, LOW);
  positions[index] = position + pulseDirection;
}

void manualMotionUpdate() {
  if (emergencyActive || mode == MotionMode::IDLE) return;

  const unsigned long nowUs = micros();
  const unsigned long elapsedUs = nowUs - lastControlUs;
  if (elapsedUs < MANUAL_CONTROL_PERIOD_US) return;
  lastControlUs = nowUs;
  const float elapsedSeconds = min(elapsedUs * 0.000001f, 0.01f);

  if (mode == MotionMode::JOG && millis() - lastHeartbeatMs >= MANUAL_JOG_HEARTBEAT_MS) {
    requestedDirection = 0;
  }

  float desiredSpeed = 0.0f;
  bool completeTarget = false;
  if (millis() - enabledAtMs >= MANUAL_ENABLE_SETTLE_MS) {
    if (mode == MotionMode::JOG) {
      desiredSpeed = requestedDirection == 0 ? 0.0f : cruiseSpeed;
    } else {
      const long position = manualMotionGetPosition(activeAxis);
      const long remaining = targetPosition - position;
      if (targetReached || remaining == 0) {
        completeTarget = true;
      } else {
        const int8_t targetDirection = remaining > 0 ? 1 : -1;
        // A target is only started from rest. This guard keeps direction changes
        // safe even if a future caller attempts to retarget while moving.
        if (targetDirection != configuredDirection) {
          desiredSpeed = 0.0f;
        } else {
          const float brakingSpeed = sqrtf(2.0f * deceleration * fabsf(static_cast<float>(remaining)));
          desiredSpeed = min(cruiseSpeed, max(MANUAL_MIN_TARGET_PPS, brakingSpeed));
        }
      }
    }
  }

  if (currentSpeed < desiredSpeed) {
    currentSpeed = min(desiredSpeed, currentSpeed + acceleration * elapsedSeconds);
  } else if (currentSpeed > desiredSpeed) {
    currentSpeed = max(desiredSpeed, currentSpeed - deceleration * elapsedSeconds);
  }
  setPulseRate(currentSpeed);

  if ((mode == MotionMode::JOG && requestedDirection == 0 && currentSpeed <= 0.5f) ||
      (mode == MotionMode::TARGET && completeTarget && currentSpeed <= 0.5f)) {
    finishMotion(mode == MotionMode::TARGET && !targetCancelled);
  }
}

bool manualMotionRequestJog(ManualAxis axis, int8_t direction) {
  if (!manualIsAxis(axis) || direction < -1 || direction > 1 || emergencyActive) return false;
  if (direction == 0) {
    if (mode == MotionMode::JOG && activeAxis == axis) {
      requestedDirection = 0;
      return true;
    }
    return mode == MotionMode::IDLE;
  }
  if (mode == MotionMode::IDLE) return startMotion(axis, MotionMode::JOG, direction, 0);
  if (mode != MotionMode::JOG || activeAxis != axis || direction != configuredDirection) return false;
  requestedDirection = direction;
  lastHeartbeatMs = millis();
  return true;
}

bool manualMotionRequestTarget(ManualAxis axis, long rawTargetPosition) {
  if (!manualIsAxis(axis) || emergencyActive || mode != MotionMode::IDLE) return false;
  const long position = manualMotionGetPosition(axis);
  if (position == rawTargetPosition) {
    lastTargetReached = true;
    Serial.print(F("ALREADY_AT_TARGET "));
    Serial.println(manualAxisName(axis));
    return true;
  }
  return startMotion(axis, MotionMode::TARGET, rawTargetPosition > position ? 1 : -1, rawTargetPosition);
}

void manualMotionNoteHeartbeat() {
  if (mode == MotionMode::JOG) lastHeartbeatMs = millis();
}

void manualMotionControlledStop() {
  if (mode == MotionMode::IDLE || emergencyActive) return;
  requestedDirection = 0;
  if (mode == MotionMode::TARGET) {
    // Convert an automatic move to a zero-speed jog so Space/STOP preserves
    // the configured deceleration instead of abruptly cutting step pulses.
    // The position counter continues to track those final braking pulses.
    targetActive = false;
    targetReached = false;
    targetCancelled = true;
    mode = MotionMode::JOG;
  }
}

void manualMotionEmergencyStop() {
  emergencyActive = true;
  MANUAL_ATOMIC {
    pulseRate = 0;
    pulseAccumulator = 0;
    targetActive = false;
    targetReached = false;
  }
  lastTargetReached = false;
  currentSpeed = 0.0f;
  requestedDirection = 0;
  mode = MotionMode::IDLE;
  activeAxis = ManualAxis::NONE;
  timerAxis = ManualAxis::NONE;
  disableAllAxes();
}

void manualMotionClearEmergency() {
  if (mode == MotionMode::IDLE) emergencyActive = false;
}

bool manualMotionIsIdle() { return mode == MotionMode::IDLE; }
bool manualMotionEmergencyActive() { return emergencyActive; }
bool manualMotionLastTargetReached() { return lastTargetReached; }
ManualAxis manualMotionActiveAxis() { return activeAxis; }

long manualMotionGetPosition(ManualAxis axis) {
  if (!manualIsAxis(axis)) return 0;
  long result = 0;
  MANUAL_ATOMIC { result = positions[axisIndex(axis)]; }
  return result;
}

void manualMotionSetPosition(ManualAxis axis, long position) {
  if (!manualIsAxis(axis) || !manualMotionIsIdle()) return;
  MANUAL_ATOMIC { positions[axisIndex(axis)] = position; }
}

int16_t manualMotionGetVelocity() {
  return static_cast<int16_t>(min(currentSpeed, 32767.0f));
}

void manualMotionPrintStatus() {
  Serial.print(F("MOTION active="));
  Serial.print(manualAxisName(activeAxis));
  Serial.print(F(" | velocity="));
  Serial.print(manualMotionGetVelocity());
  Serial.print(F(" | estop="));
  Serial.println(emergencyActive ? F("YES") : F("no"));
}
