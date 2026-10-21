#include "MotorControl.h"
#include "PositionManager.h"
#include "ALM_Monitor.h"
#include <limits.h>
#include <math.h>
#ifdef ARDUINO_ARCH_AVR
#include <avr/interrupt.h>
#include <util/atomic.h>
#define MOTION_ATOMIC ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#else
#define MOTION_ATOMIC for (bool once = true; once; once = false)
#endif

volatile bool emergencyStopTriggered = false;

struct AxisMotion {
  volatile int16_t velocity;
  uint16_t accumulator;          // ISR only, apart from initialization
  int8_t currentDirection;      // ISR only
  volatile bool targetActive;
  volatile long target;
  volatile bool complete;
  volatile bool bounded;
  float velocityF;              // main loop only
  int8_t jogDirection;
};
static AxisMotion motion[4] = {};
static float jogSpeed = DEFAULT_JOG_SPEED;
static unsigned long lastControlUs = 0;
static unsigned long lastHeartbeatMs = 0;
static bool wasMoving = false;
#ifdef ARDUINO_ARCH_AVR
static volatile uint8_t* pulsePorts[MOTOR_COUNT];
static uint8_t pulseMasks[MOTOR_COUNT];
#endif

static bool validAxis(Axis axis) { return axis >= AXIS_X && axis <= AXIS_Z; }

static void setVelocity(Axis axis, int16_t velocity) {
  MOTION_ATOMIC { motion[axis].velocity = velocity; }
}
int16_t getAxisVelocity(Axis axis) {
  if (!validAxis(axis)) return 0;
  int16_t result = 0;
  MOTION_ATOMIC { result = motion[axis].velocity; }
  return result;
}
float getJogSpeed() { return jogSpeed; }

static void setDirection(Axis axis, int8_t direction) {
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    const MotorPins& motor = MOTOR_PINS[i];
    if (motor.axis != axis) continue;
    bool cw = direction > 0 ? motor.positiveIsCW : !motor.positiveIsCW;
    digitalWrite(motor.dir, cw ? CW_LEVEL : CCW_LEVEL);
  }
  motion[axis].currentDirection = direction;
}

// Same fractional step generator as AF-434: one tick every 50 microseconds.
// No floating-point calculations, serial calls, or delays beyond pulse width.
void motorStepTick() {
  if (emergencyStopTriggered) return;
  uint8_t raised = 0;
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    AxisMotion& state = motion[i];
    int16_t velocity = state.velocity;
    if (velocity == 0) continue;
    int8_t direction = velocity > 0 ? 1 : -1;
    if (direction != state.currentDirection) {
      setDirection((Axis)i, direction);
      continue;  // one full tick for DIR setup before the next pulse
    }
    state.accumulator += (uint16_t)(velocity > 0 ? velocity : -velocity);
    if (state.accumulator < STEP_TICK_HZ) continue;
    state.accumulator -= STEP_TICK_HZ;
    long position = getCurrentPosition((Axis)i);
    bool atTarget = state.targetActive &&
        (direction > 0 ? position >= state.target : position <= state.target);
    bool atBound = state.bounded &&
        (direction > 0 ? position >= getMaxPosition((Axis)i) : position <= 0);
    if (atTarget || atBound || (direction > 0 ? position == LONG_MAX : position == LONG_MIN)) {
      state.velocity = 0;
      state.complete = true;
      continue;
    }
    raised |= (1 << i);
    updatePosition((Axis)i, direction);
  }
  // Raise every selected motor before lowering any, including BOTH X outputs.
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    if (!(raised & (1 << MOTOR_PINS[i].axis))) continue;
#ifdef ARDUINO_ARCH_AVR
    *pulsePorts[i] |= pulseMasks[i];
#else
    digitalWrite(MOTOR_PINS[i].pul, HIGH);
#endif
  }
  if (raised) delayMicroseconds(STEP_PULSE_WIDTH_US);
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    if (!(raised & (1 << MOTOR_PINS[i].axis))) continue;
#ifdef ARDUINO_ARCH_AVR
    *pulsePorts[i] &= (uint8_t)~pulseMasks[i];
#else
    digitalWrite(MOTOR_PINS[i].pul, LOW);
#endif
  }
}

#ifdef ARDUINO_ARCH_AVR
ISR(TIMER1_COMPA_vect) { motorStepTick(); }
#endif

void initializeMotor() {
#ifdef ARDUINO_ARCH_AVR
  TIMSK1 = 0;
#endif
  emergencyStopTriggered = false;
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    motion[i].velocity = 0;
    motion[i].accumulator = 0;
    motion[i].targetActive = false;
    motion[i].target = 0;
    motion[i].complete = false;
    motion[i].bounded = false;
    motion[i].velocityF = 0;
    motion[i].jogDirection = 0;
    setDirection((Axis)i, 1);
  }
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    const MotorPins& motor = MOTOR_PINS[i];
    digitalWrite(motor.pul, LOW);
    pinMode(motor.pul, OUTPUT);
    pinMode(motor.dir, OUTPUT);
    digitalWrite(motor.ena, ENA_ENABLED_LEVEL);
    pinMode(motor.ena, OUTPUT);
#ifdef ARDUINO_ARCH_AVR
    pulsePorts[i] = portOutputRegister(digitalPinToPort(motor.pul));
    pulseMasks[i] = digitalPinToBitMask(motor.pul);
#endif
  }
  jogSpeed = DEFAULT_JOG_SPEED;
  wasMoving = false;
  lastControlUs = micros();
  lastHeartbeatMs = millis();
  initializeALM();
#ifdef ARDUINO_ARCH_AVR
  MOTION_ATOMIC {
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    OCR1A = (uint16_t)(F_CPU / STEP_TICK_HZ) - 1;
    TCCR1B = (1 << WGM12) | (1 << CS10);
    TIMSK1 = (1 << OCIE1A);
  }
#endif
  Serial.println(F("Four motors: M1=X-left, M2=X-right, M3=Y, M4=Z; Timer1 ramps active"));
}

bool anyAxisMoving() {
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    if (motion[i].velocityF != 0 || getAxisVelocity((Axis)i) != 0 ||
        motion[i].jogDirection != 0 || motion[i].targetActive) return true;
  }
  return false;
}
void noteJogHeartbeat() { lastHeartbeatMs = millis(); }

void jogAxis(Axis axis, int8_t direction) {
  if (!validAxis(axis) || direction < -1 || direction > 1) return;
  if (direction && isEmergencyStopActive()) {
    Serial.println(F("ERR emergency stop active; send CLEAR"));
    return;
  }
  // Releasing a key must never cancel a finite position move.
  if (motion[axis].targetActive) {
    if (direction) Serial.println(F("ERR position move active; stop before jogging"));
    return;
  }
  motion[axis].bounded = isPositionReferenced(axis) && getMaxPosition(axis) > 0;
  motion[axis].jogDirection = direction;
  if (direction) noteJogHeartbeat();
}

void smoothStopAll() {
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    motion[i].jogDirection = 0;
    MOTION_ATOMIC { motion[i].targetActive = false; }
  }
  if (!anyAxisMoving()) Serial.println(F("STOPPED"));
}

void changeJogSpeed(bool faster) {
  jogSpeed = constrain(faster ? jogSpeed * JOG_SPEED_FACTOR : jogSpeed / JOG_SPEED_FACTOR,
                       MIN_JOG_SPEED, MAX_JOG_SPEED);
  Serial.print(F("SPEED "));
  Serial.println((long)jogSpeed);
}

static float brakingSpeed(float distance) {
  float speed = sqrtf(2.0f * MOTION_ACCELERATION * 0.85f * distance);
  return min(jogSpeed, max(CRAWL_SPEED, speed));
}

void updateMotorControl() {
  unsigned long now = micros();
  unsigned long elapsed = now - lastControlUs;
  if (elapsed < CONTROL_PERIOD_US) return;
  lastControlUs = now;
  float dt = min(elapsed * 1e-6f, 0.01f);
  if (millis() - lastHeartbeatMs >= JOG_TIMEOUT_MS) {
    for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) motion[i].jogDirection = 0;
  }
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    AxisMotion& state = motion[i];
    if (emergencyStopTriggered) continue;
    if (state.complete) {
      MOTION_ATOMIC {
        state.velocity = 0;
        state.complete = false;
        state.targetActive = false;
      }
      state.velocityF = 0;
      state.jogDirection = 0;
      Serial.print(F("DONE "));
      Serial.println((char)('X' + i - AXIS_X));
      continue;
    }
    float targetVelocity = state.jogDirection * jogSpeed;
    long position = getCurrentPosition((Axis)i);
    if (state.targetActive) {
      // Use 64 bits for subtraction so far-apart signed positions cannot wrap.
      int64_t remaining = (int64_t)state.target - position;
      if (remaining == 0) {
        MOTION_ATOMIC { state.complete = true; state.velocity = 0; }
        continue;
      }
      float distance = (float)(remaining > 0 ? remaining : -remaining);
      targetVelocity = brakingSpeed(distance) * (remaining > 0 ? 1 : -1);
    } else if (state.bounded && state.jogDirection) {
      long remaining = state.jogDirection > 0 ? getMaxPosition((Axis)i) - position : position;
      targetVelocity = remaining > 0 ? brakingSpeed((float)remaining) * state.jogDirection : 0;
    }
    // Come all the way to zero before accelerating in the opposite direction.
    if ((state.velocityF > 0 && targetVelocity < 0) ||
        (state.velocityF < 0 && targetVelocity > 0)) targetVelocity = 0;
    float change = MOTION_ACCELERATION * dt;
    if (state.velocityF < targetVelocity) state.velocityF = min(state.velocityF + change, targetVelocity);
    else if (state.velocityF > targetVelocity) state.velocityF = max(state.velocityF - change, targetVelocity);
    setVelocity((Axis)i, (int16_t)state.velocityF);
  }
  bool moving = anyAxisMoving();
  if (wasMoving && !moving) Serial.println(F("STOPPED"));
  wasMoving = moving;
}

void emergencyStop() {
  bool alreadyStopped = emergencyStopTriggered;
  MOTION_ATOMIC {
    emergencyStopTriggered = true;
    for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
      motion[i].velocity = 0;
      motion[i].accumulator = 0;
      motion[i].targetActive = false;
      motion[i].complete = false;
    }
  }
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    motion[i].jogDirection = 0;
    motion[i].velocityF = 0;
    setPositionReferenced((Axis)i, false);
  }
  if (!alreadyStopped) {
    Serial.println(F("EMERGENCY STOP ACTIVATED - pulses stopped, holding torque retained"));
    Serial.println(F("Fix any fault, then CLEAR or S0. Set a manual zero with 0 again."));
  }
}

void resumeOperations() {
  if (!clearAllALM()) {
    emergencyStop();
    Serial.println(F("Cannot clear: a driver ALM input is still active"));
    return;
  }
  emergencyStopTriggered = false;
  Serial.println(F("Emergency stop cleared - operations resumed"));
}

static void startPositionMove(Axis axis, long target) {
  if (anyAxisMoving()) {
    Serial.println(F("ERR busy moving; wait for STOPPED or stop first"));
    return;
  }
  AxisMotion& state = motion[axis];
  state.velocityF = 0;
  state.jogDirection = 0;
  MOTION_ATOMIC {
    state.velocity = 0;
    state.accumulator = 0;
    state.target = target;
    state.complete = false;
    state.bounded = isPositionReferenced(axis) && getMaxPosition(axis) > 0;
    state.targetActive = target != getCurrentPosition(axis);
  }
  Serial.print(F("TARGET "));
  Serial.print((char)('X' + axis - AXIS_X));
  Serial.print(' ');
  Serial.println(target);
}

void processRelativeMove(Axis axis, long steps) {
  if (!validAxis(axis) || isEmergencyStopActive()) return;
  int64_t target = (int64_t)getCurrentPosition(axis) + steps;
  if (isPositionReferenced(axis) && getMaxPosition(axis) > 0) {
    if (target < 0) target = 0;
    if (target > getMaxPosition(axis)) target = getMaxPosition(axis);
  }
  if (target > LONG_MAX || target < LONG_MIN) {
    Serial.println(F("Position counter range exceeded"));
    return;
  }
  startPositionMove(axis, (long)target);
}

void processAbsoluteMove(Axis axis, int percentage) {
  if (!validAxis(axis) || isEmergencyStopActive()) return;
  if (percentage != 25 && percentage != 50 && percentage != 75) return;
  if (!isPositionReferenced(axis) || getMaxPosition(axis) <= 0) {
    Serial.println(F("Percentage moves require measured *_TRAVEL_STEPS settings and a manual 0"));
    return;
  }
  startPositionMove(axis, (long)((int64_t)getMaxPosition(axis) * percentage / 100));
}

bool isEmergencyStopActive() { return emergencyStopTriggered || isAnyALMCritical(); }
