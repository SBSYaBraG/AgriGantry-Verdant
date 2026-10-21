/*
  AgriGantry - COMPLETE SINGLE-FILE ARDUINO FIRMWARE

  Upload THIS file to the Arduino, just like the AF-434 sketch:
    1. Open AgriGantry.ino in Arduino IDE.
    2. Select Arduino Mega or Mega 2560 and its port.
    3. Click Upload.

  Then run Pi/gantry_keyboard.py on your COMPUTER for keyboard control.
  W/S = X, A/D = Y, Q/E = Z; release to slow down; O = emergency stop.
  Hardware: M1=X-left, M2=X-right, M3=Y, M4=Z. No limit switches.

  Everything the Arduino needs from this project is in this one file.
  Pin assignments and optional travel settings are directly below.
*/
#include <Arduino.h>

// ---------------- Config.h ----------------
/** AF-434 hardware mapping with AgriGantry's three-axis command interface. */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

enum Axis { AXIS_NONE = 0, AXIS_X = 1, AXIS_Y = 2, AXIS_Z = 3 };
enum Motor { MOTOR_XL = 0, MOTOR_XR = 1, MOTOR_Y = 2, MOTOR_Z = 3, MOTOR_COUNT = 4 };

// Copied from AF-434/gantry_control/gantry_control.ino, commit 873e2b6.
// Follow the executable sketch: its X direction settings differ from its README.
#define XL_PUL_PIN 9
#define XL_DIR_PIN 8
#define XL_ENA_PIN 22
#define ALM_XL_PIN 26
#define XR_PUL_PIN 7
#define XR_DIR_PIN 6
#define XR_ENA_PIN 23
#define ALM_XR_PIN 27
#define Y_PUL_PIN 5
#define Y_DIR_PIN 4
#define Y_ENA_PIN 24
#define ALM_Y_PIN 28
#define Z_PUL_PIN 3
#define Z_DIR_PIN 2
#define Z_ENA_PIN 25
#define ALM_Z_PIN 29

#define ENA_ENABLED_LEVEL LOW
#define ENA_DISABLED_LEVEL HIGH
#define ALM_ACTIVE_LEVEL HIGH
#define ALM_DEBOUNCE_MS 5
#define CW_LEVEL HIGH
#define CCW_LEVEL LOW
#define XL_POSITIVE_IS_CW true
#define XR_POSITIVE_IS_CW true
#define Y_POSITIVE_IS_CW true
#define Z_POSITIVE_IS_CW false

struct MotorPins {
  Axis axis;
  uint8_t pul, dir, ena, alm;
  bool positiveIsCW;
};
static const MotorPins MOTOR_PINS[MOTOR_COUNT] = {
  {AXIS_X, XL_PUL_PIN, XL_DIR_PIN, XL_ENA_PIN, ALM_XL_PIN, XL_POSITIVE_IS_CW},
  {AXIS_X, XR_PUL_PIN, XR_DIR_PIN, XR_ENA_PIN, ALM_XR_PIN, XR_POSITIVE_IS_CW},
  {AXIS_Y, Y_PUL_PIN, Y_DIR_PIN, Y_ENA_PIN, ALM_Y_PIN, Y_POSITIVE_IS_CW},
  {AXIS_Z, Z_PUL_PIN, Z_DIR_PIN, Z_ENA_PIN, ALM_Z_PIN, Z_POSITIVE_IS_CW}
};

// No limit switches are installed. Set measured usable travel in pulses here
// to use percentage positioning and software bounds after a manual 0 command.
// Zero means unknown; do not invent a physical travel length.
#ifndef X_TRAVEL_STEPS
#define X_TRAVEL_STEPS 0L
#endif
#ifndef Y_TRAVEL_STEPS
#define Y_TRAVEL_STEPS 0L
#endif
#ifndef Z_TRAVEL_STEPS
#define Z_TRAVEL_STEPS 0L
#endif
static_assert(X_TRAVEL_STEPS >= 0 && Y_TRAVEL_STEPS >= 0 && Z_TRAVEL_STEPS >= 0,
              "Travel lengths must be nonnegative step counts");

// AF-434 velocity ramp and Timer1 step generator. Units are pulses and seconds.
#define STEP_TICK_HZ 20000U
#define STEP_PULSE_WIDTH_US 3
#define DEFAULT_JOG_SPEED 1000.0f
#define MIN_JOG_SPEED 100.0f
#define MAX_JOG_SPEED 8000.0f
#define JOG_SPEED_FACTOR 1.25f
#define MOTION_ACCELERATION 3000.0f
#define CRAWL_SPEED 150.0f
#define CONTROL_PERIOD_US 1000UL
#define JOG_TIMEOUT_MS 500UL
#define SERIAL_BAUD_RATE 115200
#define FIRMWARE_ID "AGRIGANTRY-KEYBOARD-v1"

#endif

// ---------------- PositionManager.h ----------------
#ifndef POSITION_MANAGER_H
#define POSITION_MANAGER_H

void updatePosition(Axis axis, long steps);
long getCurrentPosition(Axis axis);
void setCurrentPosition(Axis axis, long position);
long getMaxPosition(Axis axis);
int getPositionPercentage(Axis axis);
bool isPositionReferenced(Axis axis);
void setPositionReferenced(Axis axis, bool referenced);

struct MotorALM {
  bool signal;
  bool lastSignal;
  unsigned long signalChangedAt;
  bool critical;
};
MotorALM& getMotorALM(Motor motor);
void initializeALM();
bool clearAllALM();
#endif

// ---------------- MotorControl.h ----------------
#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

void initializeMotor();
void updateMotorControl();
void motorStepTick();  // Timer1 ISR entry; also driven by the native test clock.
bool anyAxisMoving();
int16_t getAxisVelocity(Axis axis);
float getJogSpeed();
void changeJogSpeed(bool faster);
void jogAxis(Axis axis, int8_t direction);
void noteJogHeartbeat();
void smoothStopAll();
void emergencyStop();
void resumeOperations();
void processRelativeMove(Axis axis, long steps);
void processAbsoluteMove(Axis axis, int percentage);
bool isEmergencyStopActive();
#endif

// ---------------- ALM_Monitor.h ----------------
#ifndef ALM_MONITOR_H
#define ALM_MONITOR_H

void monitorAllALM();
void monitorMotorALM(Motor motor);
bool isAnyALMCritical();
bool isMotorALMCritical(Motor motor);
void printALMStatus();
const char* getMotorName(Motor motor);
#endif

// ---------------- SystemOperations.h ----------------
#ifndef SYSTEM_OPERATIONS_H
#define SYSTEM_OPERATIONS_H

void runHoming(Axis axis);
void runHomingAll();
void zeroPosition(Axis axis = AXIS_NONE);
#endif

// ---------------- CommandProcessor.h ----------------
#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H
#include <Arduino.h>

void processCommand(String command);
void pollSerialCommands();
void reportStatus();
bool checkForEmergencyStop();
#endif

// Internal declarations
static bool validAxis(Axis axis);
static void setVelocity(Axis axis, int16_t velocity);
static void setDirection(Axis axis, int8_t direction);
static float brakingSpeed(float distance);
static void startPositionMove(Axis axis, long target);
static Axis axisFromChar(char c);
static bool parseNumber(const String& text, long& value);
static void consumeSerial(bool allowMotionCommands);
void setup();
void loop();

// ---------------- PositionManager.cpp ----------------
#ifdef ARDUINO_ARCH_AVR
#include <util/atomic.h>
#define POSITION_ATOMIC ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#else
#define POSITION_ATOMIC for (bool once = true; once; once = false)
#endif

static volatile long currentPosition[4] = {0, 0, 0, 0};
static const long maxPosition[4] = {0, X_TRAVEL_STEPS, Y_TRAVEL_STEPS, Z_TRAVEL_STEPS};
static bool positionReferenced[4] = {false, false, false, false};
static MotorALM almStates[MOTOR_COUNT];

void updatePosition(Axis axis, long steps) {
  if (axis >= AXIS_X && axis <= AXIS_Z) {
    POSITION_ATOMIC { currentPosition[axis] += steps; }
  }
}
long getCurrentPosition(Axis axis) {
  long result = 0;
  if (axis >= AXIS_X && axis <= AXIS_Z) {
    POSITION_ATOMIC { result = currentPosition[axis]; }
  }
  return result;
}
void setCurrentPosition(Axis axis, long position) {
  if (axis >= AXIS_X && axis <= AXIS_Z) {
    POSITION_ATOMIC { currentPosition[axis] = position; }
  }
}
long getMaxPosition(Axis axis) {
  return axis >= AXIS_X && axis <= AXIS_Z ? maxPosition[axis] : 0;
}
int getPositionPercentage(Axis axis) {
  long maximum = getMaxPosition(axis);
  if (maximum <= 0) return 0;
  int64_t percent = (int64_t)getCurrentPosition(axis) * 100 / maximum;
  return (int)constrain(percent, 0, 100);
}
bool isPositionReferenced(Axis axis) {
  return axis >= AXIS_X && axis <= AXIS_Z && positionReferenced[axis];
}
void setPositionReferenced(Axis axis, bool referenced) {
  if (axis >= AXIS_X && axis <= AXIS_Z) positionReferenced[axis] = referenced;
}
MotorALM& getMotorALM(Motor motor) {
  return almStates[motor >= MOTOR_XL && motor < MOTOR_COUNT ? motor : MOTOR_XL];
}
void initializeALM() {
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    pinMode(MOTOR_PINS[i].alm, INPUT_PULLUP);
    bool signal = digitalRead(MOTOR_PINS[i].alm);
    almStates[i] = {signal, signal, millis(), false};
  }
}
bool clearAllALM() {
  bool clear = true;
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    MotorALM& alm = almStates[i];
    alm.signal = digitalRead(MOTOR_PINS[i].alm);
    alm.lastSignal = alm.signal;
    alm.signalChangedAt = millis();
    alm.critical = (alm.signal == ALM_ACTIVE_LEVEL);
    if (alm.critical) clear = false;
  }
  return clear;
}

// ---------------- ALM_Monitor.cpp ----------------
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

// ---------------- MotorControl.cpp ----------------
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

// ---------------- SystemOperation.cpp ----------------
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

// ---------------- CommandProcessor.cpp ----------------
#include <stdlib.h>
#include <errno.h>

static char inputBuffer[48];
static uint8_t inputLength = 0;
static bool inputOverflow = false;
static bool discardLine = false;

static Axis axisFromChar(char c) {
  return c == 'X' ? AXIS_X : c == 'Y' ? AXIS_Y : c == 'Z' ? AXIS_Z : AXIS_NONE;
}

static bool parseNumber(const String& text, long& value) {
  if (text.length() == 0) return false;
  char* end = 0;
  errno = 0;
  value = strtol(text.c_str(), &end, 10);
  return errno != ERANGE && end != text.c_str() && *end == '\0';
}

void processCommand(String command) {
  command.trim();
  command.toUpperCase();
  if (command.length() == 0) return;

  if (command == "?") {
    Serial.println(F(FIRMWARE_ID));
  } else if (command == "HB") {
    noteJogHeartbeat();
  } else if (command == "J0") {
    smoothStopAll();
  } else if (command == "J+" || command == "J-") {
    changeJogSpeed(command[1] == '+');
  } else if (command.length() >= 3 && command[0] == 'J' && axisFromChar(command[1]) != AXIS_NONE) {
    long direction;
    if (parseNumber(command.substring(2), direction) && direction >= -1 && direction <= 1) {
      jogAxis(axisFromChar(command[1]), (int8_t)direction);
    } else {
      Serial.println(F("Invalid jog direction; use -1, 0, or 1"));
    }
  } else if (command == "O" || command == "S" || command == "STOP" || command == "!") {
    emergencyStop();
  } else if (command == "S0" || command == "CLEAR") {
    resumeOperations();
  } else if (command == "R" || command == "STATUS") {
    reportStatus();
  } else if (command == "ALM") {
    printALMStatus();
  } else if (command == "0") {
    zeroPosition();
  } else if (command.length() == 2 && command[0] == '0' && axisFromChar(command[1]) != AXIS_NONE) {
    zeroPosition(axisFromChar(command[1]));
  } else if (command == "H" || command == "HALL" || command == "HX" || command == "HY" || command == "HZ") {
    runHomingAll();
  } else if (axisFromChar(command[0]) != AXIS_NONE) {
    long steps;
    if (parseNumber(command.substring(1), steps)) {
      processRelativeMove(axisFromChar(command[0]), steps);
    } else {
      Serial.println(F("Invalid step count. Example: X1000 or Y-500"));
    }
  } else if (command.length() >= 3 && command[0] == 'P' && axisFromChar(command[1]) != AXIS_NONE) {
    long percent;
    if (parseNumber(command.substring(2), percent) && (percent == 25 || percent == 50 || percent == 75)) {
      processAbsoluteMove(axisFromChar(command[1]), (int)percent);
    } else {
      Serial.println(F("Invalid percentage. Example: PX25, PY50, or PZ75"));
    }
  } else if (command == "HELP") {
    Serial.println(F("X1000 / Y-500 / Z100: relative axis steps; one axis at a time"));
    Serial.println(F("PX25 / PY50 / PZ75: percentage of configured travel after manual 0"));
    Serial.println(F("0: record all current positions as the origin; 0X / 0Y / 0Z: one axis"));
    Serial.println(F("O: emergency stop ALL motors immediately, no newline needed"));
    Serial.println(F("Keyboard: Pi/gantry_keyboard.py; W/S X, A/D Y, Q/E Z, Space smooth stop"));
    Serial.println(F("S / STOP / ! also stop; CLEAR / S0: clear after fixing faults"));
    Serial.println(F("STATUS / R: position, reference, travel; ALM: driver alarms; HELP: commands"));
    Serial.println(F("No limit switches or automatic homing. H/HX/HY/HZ/HALL do not move."));
    Serial.println(F("Commands end with newline; O and ! stop without newline. O is reserved."));
  } else {
    Serial.println(F("Unknown command. Type HELP"));
  }
}

void reportStatus() {
  Serial.println(F("AgriGantry / AF-434 wiring: four motors, no limit switches"));
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    Axis axis = (Axis)i;
    Serial.print((char)('X' + i - AXIS_X));
    Serial.print(F(": position="));
    Serial.print(getCurrentPosition(axis));
    Serial.print(F(" | velocity="));
    Serial.print(getAxisVelocity(axis));
    Serial.print(isPositionReferenced(axis) ? F(" | manual zero set") : F(" | unreferenced"));
    Serial.print(F(" | travel="));
    Serial.print(getMaxPosition(axis));
    if (isPositionReferenced(axis) && getMaxPosition(axis) > 0) {
      Serial.print(F(" | percent="));
      Serial.print(getPositionPercentage(axis));
    }
    Serial.println();
  }
  Serial.println(isEmergencyStopActive() ? F("EMERGENCY STOP active") : F("Ready"));
  Serial.print(F("Jog speed="));
  Serial.println((long)getJogSpeed());
}

static void consumeSerial(bool allowMotionCommands) {
  // Shared nonblocking parser: a partial line must never pause motor pulses.
  // Clear the buffer BEFORE dispatch so checks during a move can parse STOP.
  for (uint8_t i = 0; i < 32 && Serial.available(); ++i) {
    char c = (char)Serial.read();
    if (c == '!' || c == 'O' || c == 'o') {
      discardLine = discardLine || inputLength > 0 || inputOverflow;
      inputLength = 0;
      inputOverflow = false;
      emergencyStop();
    } else if (c == '\n' || c == '\r') {
      if (discardLine) {
        discardLine = false;
        inputLength = 0;
        inputOverflow = false;
        continue;
      }
      if (inputOverflow) {
        inputLength = 0;
        inputOverflow = false;
        Serial.println(F("Command too long; discarded"));
      } else if (inputLength) {
        inputBuffer[inputLength] = '\0';
        String command(inputBuffer);
        command.trim();
        command.toUpperCase();
        inputLength = 0;
        if (allowMotionCommands || command == "S" || command == "STOP" || command == "O") {
          processCommand(command);
        } else {
          Serial.println(F("Busy moving; press O to stop"));
        }
      }
    } else if (!inputOverflow && !discardLine) {
      if (inputLength < sizeof(inputBuffer) - 1) inputBuffer[inputLength++] = c;
      else inputOverflow = true;
    }
  }
}

void pollSerialCommands() {
  consumeSerial(true);
}
bool checkForEmergencyStop() {
  consumeSerial(false);
  return isEmergencyStopActive();
}

// ---------------- main.cpp ----------------
/** AgriGantry commands on AF-434's working four-motor Mega 2560 wiring. */

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  initializeMotor();
  Serial.println(F("AgriGantry controller - AF-434 hardware"));
  Serial.println(F("No limit switches: no startup motion or automatic homing."));
  Serial.println(F("Relative commands: X1000, Y-500, Z100. Send HELP for all commands."));
  Serial.println(F("Press O in the Python controller to stop all motors immediately."));
  Serial.println(F("Keyboard jogging: run Pi/gantry_keyboard.py; release keys to ramp to a stop."));
  Serial.println(F("Percentage moves need measured travel settings and a manual 0."));
}

void loop() {
  monitorAllALM();
  pollSerialCommands();
  updateMotorControl();
}
