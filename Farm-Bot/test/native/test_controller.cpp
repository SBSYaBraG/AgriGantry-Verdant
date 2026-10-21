#include "Arduino.h"
#include "Config.h"
#include "MotorControl.h"
#include "PositionManager.h"
#include "CommandProcessor.h"
#include "SystemOperations.h"
#include "ALM_Monitor.h"
#include <cstdlib>
#include <iostream>
#include <limits.h>

FakeSerial Serial;
uint8_t pinLevels[70] = {};
uint8_t pinModes[70] = {};
unsigned long pulseCount[70] = {};
std::vector<PinWrite> writes;
uint64_t fakeTimeUs = 0;
std::function<void()> onAdvance;
std::function<void(uint8_t)> onPulse;
void setup();
void loop();

void pinMode(uint8_t pin, uint8_t mode) { pinModes[pin] = mode; }
int digitalRead(uint8_t pin) { return pinLevels[pin]; }
void digitalWrite(uint8_t pin, uint8_t level) {
  writes.push_back({pin, level});
  bool rising = !pinLevels[pin] && level;
  pinLevels[pin] = level;
  if (rising && (pin == 9 || pin == 7 || pin == 5 || pin == 3)) {
    ++pulseCount[pin];
    if (onPulse) onPulse(pin);
  }
}
unsigned long millis() { return (unsigned long)(fakeTimeUs / 1000); }
unsigned long micros() { return (unsigned long)fakeTimeUs; }
void delayMicroseconds(unsigned int us) { fakeTimeUs += us; }
void delay(unsigned long ms) { fakeTimeUs += (uint64_t)ms * 1000; }

static int checks = 0;
static unsigned long lastTestHeartbeat = 0;
static void require(bool condition, const char* reason) {
  ++checks;
  if (!condition) { std::cerr << "FAIL: " << reason << '\n'; std::exit(1); }
}
static void advanceMs(unsigned long duration, bool heartbeat = true) {
  uint64_t end = fakeTimeUs + duration * 1000ULL;
  while (fakeTimeUs < end) {
    fakeTimeUs += 50 - fakeTimeUs % 50;
    motorStepTick();
    if (heartbeat && millis() - lastTestHeartbeat >= 100) {
      Serial.inject("HB\n");
      lastTestHeartbeat = millis();
    }
    loop();
  }
}
static void finishMove() {
  unsigned long start = millis();
  while (anyAxisMoving() && millis() - start < 20000) advanceMs(1, false);
  require(!anyAxisMoving(), "finite move or ramp finishes within the test timeout");
}
static void reset() {
  onPulse = nullptr;
  Serial.input.clear();
  Serial.inject("\n");
  pollSerialCommands();
  Serial.output.clear();
  std::fill(pinLevels, pinLevels + 70, (uint8_t)0);
  std::fill(pulseCount, pulseCount + 70, 0UL);
  writes.clear();
  for (int i = AXIS_X; i <= AXIS_Z; ++i) {
    setCurrentPosition((Axis)i, 0);
    setPositionReferenced((Axis)i, false);
  }
  fakeTimeUs = 0;
  lastTestHeartbeat = 0;
  setup();
}

int main() {
  reset();
  const uint8_t expected[4][4] = {{9,8,22,26},{7,6,23,27},{5,4,24,28},{3,2,25,29}};
  for (int i = 0; i < MOTOR_COUNT; ++i) {
    require(MOTOR_PINS[i].pul == expected[i][0] && MOTOR_PINS[i].dir == expected[i][1]
            && MOTOR_PINS[i].ena == expected[i][2] && MOTOR_PINS[i].alm == expected[i][3], "AF-434 motor pins");
    require(pinLevels[MOTOR_PINS[i].ena] == LOW, "active-LOW enables retain torque");
    require(pinModes[MOTOR_PINS[i].alm] == INPUT_PULLUP, "ALM inputs use pullups");
  }
  advanceMs(100);
  require(pulseCount[9] + pulseCount[7] + pulseCount[5] + pulseCount[3] == 0, "startup never moves");

  for (long count : {0L, 1L, 2L, 9L, 10L, 11L, 19L, 20L, 21L, 30L, 50L, 99L, 100L}) {
    reset();
    processRelativeMove(AXIS_X, count);
    finishMove();
    require(pulseCount[9] == (unsigned long)count && pulseCount[7] == (unsigned long)count, "both X motors get exact finite pulse counts");
    require(pulseCount[5] == 0 && pulseCount[3] == 0, "finite X command leaves Y and Z stationary");
    require(getCurrentPosition(AXIS_X) == count, "X position counted once per pair");
    require(pinLevels[8] == HIGH && pinLevels[6] == HIGH, "actual AF-434 X positive directions");
  }
  reset();
  processCommand("Y-5");
  finishMove();
  require(getCurrentPosition(AXIS_Y) == -5 && pulseCount[5] == 5, "negative relative moves work without homing");
  processCommand("Z3");
  finishMove();
  require(pinLevels[2] == LOW && getCurrentPosition(AXIS_Z) == 3, "positive Z direction from AF-434");
  processCommand("Z-2");
  finishMove();
  require(pinLevels[2] == HIGH && getCurrentPosition(AXIS_Z) == 1, "negative Z reverses DIR");
  require(pinLevels[25] == LOW, "Z retains holding torque");

  reset();
  Serial.inject("JX1\nJY1\nJZ1\n");
  pollSerialCommands();
  advanceMs(400);
  require(getAxisVelocity(AXIS_X) == 1000 && getAxisVelocity(AXIS_Y) == 1000 && getAxisVelocity(AXIS_Z) == 1000,
          "all axes jog simultaneously at the default AF-434 speed");
  require(pulseCount[9] == pulseCount[7] && pulseCount[5] > 0 && pulseCount[3] > 0, "simultaneous jogging pulses both X motors and Y/Z");
  Serial.inject("JX0\n");
  pollSerialCommands();
  int previous = getAxisVelocity(AXIS_X);
  long releasePosition = getCurrentPosition(AXIS_X);
  require(previous > 0, "releasing a key does not snap velocity to zero");
  for (int i = 0; i < 350; ++i) {
    advanceMs(1);
    int velocity = getAxisVelocity(AXIS_X);
    require(velocity >= 0 && velocity <= previous && previous - velocity <= 4, "key release decelerates monotonically at the configured rate");
    previous = velocity;
  }
  require(previous == 0 && getCurrentPosition(AXIS_X) > releasePosition, "release travels through its braking distance to rest");
  require(getAxisVelocity(AXIS_Y) == 1000 && getAxisVelocity(AXIS_Z) == 1000, "releasing X leaves other held axes moving");
  processCommand("J0");
  require(getAxisVelocity(AXIS_Y) > 0 && !isEmergencyStopActive(), "Space requests a normal ramp stop, without latching emergency");
  finishMove();

  reset();
  processCommand("JX1");
  advanceMs(400);
  processCommand("JX-1");
  bool sawZero = false;
  int previousVelocity = getAxisVelocity(AXIS_X);
  for (int i = 0; i < 750; ++i) {
    advanceMs(1);
    int velocity = getAxisVelocity(AXIS_X);
    require(abs(velocity - previousVelocity) <= 4, "direction reversal follows the acceleration limit");
    if (velocity == 0) sawZero = true;
    if (velocity < 0) require(sawZero, "reversal reaches zero before becoming negative");
    previousVelocity = velocity;
  }
  require(sawZero && getAxisVelocity(AXIS_X) == -1000 && pinLevels[8] == LOW && pinLevels[6] == LOW,
          "reversed X jogging accelerates smoothly in the correct direction");

  reset();
  processCommand("JX1");
  advanceMs(400, false);
  require(getAxisVelocity(AXIS_X) == 1000, "jog runs while within heartbeat timeout");
  advanceMs(110, false);
  require(getAxisVelocity(AXIS_X) > 0 && getAxisVelocity(AXIS_X) < 1000, "500ms silence starts a ramp rather than an abrupt stop");
  advanceMs(400, false);
  require(!anyAxisMoving() && !isEmergencyStopActive(), "lost keyboard heartbeat brings motion to rest");

  reset();
  processCommand("X2000");
  int peak = 0;
  bool slowedBeforeTarget = false;
  while (anyAxisMoving() && millis() < 10000) {
    advanceMs(1, false);
    int velocity = getAxisVelocity(AXIS_X);
    peak = max(peak, velocity);
    if (peak == 1000 && velocity > 0 && velocity < 500 && getCurrentPosition(AXIS_X) < 2000) slowedBeforeTarget = true;
  }
  require(!anyAxisMoving() && peak == 1000 && slowedBeforeTarget, "finite moves brake on approach to the target");
  require(getCurrentPosition(AXIS_X) == 2000 && pulseCount[9] == 2000 && pulseCount[7] == 2000,
          "finite move finishes exactly, without requiring keyboard heartbeats");

  reset();
  processCommand("JX1");
  processCommand("JY1");
  processCommand("JZ1");
  advanceMs(200);
  Serial.inject("o");
  pollSerialCommands();
  require(isEmergencyStopActive() && !anyAxisMoving(), "O immediately zeros every axis without newline");
  unsigned long counts[4] = {pulseCount[9], pulseCount[7], pulseCount[5], pulseCount[3]};
  advanceMs(500);
  require(pulseCount[9] == counts[0] && pulseCount[7] == counts[1] && pulseCount[5] == counts[2] && pulseCount[3] == counts[3],
          "no motor receives further pulses after O");
  processCommand("JY1");
  processCommand("X10");
  require(!anyAxisMoving(), "emergency latch blocks jog and position moves");
  processCommand("CLEAR");
  advanceMs(100);
  require(!anyAxisMoving() && !isEmergencyStopActive(), "clearing the stop never resumes old motion");

  reset();
  Serial.inject("X12");
  pollSerialCommands();
  Serial.inject("O");
  pollSerialCommands();
  require(isEmergencyStopActive(), "O preempts a partial line");
  Serial.inject("\nCLEAR\n");
  pollSerialCommands();
  require(!isEmergencyStopActive(), "parser recovers after interrupted line");
  Serial.inject(std::string(60, '9') + "o\n");
  while (Serial.available()) pollSerialCommands();
  require(isEmergencyStopActive(), "O preempts an overflowing line");

  reset();
  processCommand("0");
  pinLevels[28] = HIGH;
  monitorAllALM();
  delay(4);
  monitorAllALM();
  require(!isEmergencyStopActive(), "ALM ignores shorter than 5ms transient");
  delay(1);
  monitorAllALM();
  require(isEmergencyStopActive() && isMotorALMCritical(MOTOR_Y), "active-HIGH ALM latches global stop");
  require(!isPositionReferenced(AXIS_X), "alarm invalidates manual references");
  processCommand("CLEAR");
  require(isEmergencyStopActive(), "cannot clear a live ALM fault");
  pinLevels[28] = LOW;
  processCommand("S0");
  require(!isEmergencyStopActive(), "can clear after the fault disappears");
  processCommand("JZ1");
  advanceMs(100);
  pinLevels[26] = HIGH;
  advanceMs(10);
  require(isEmergencyStopActive() && !anyAxisMoving(), "X driver fault stops a moving Z axis");

  reset();
  for (const char* command : {"H", "HX", "HY", "HZ", "HALL", "Xjunk", "X1junk", "X999999999999999999999999", "PX65561", "PX50junk", "JX2"}) processCommand(command);
  advanceMs(100);
  require(!anyAxisMoving() && pulseCount[9] == 0, "homing and malformed commands cannot move hardware");
  processCommand("PX50");
  require(!anyAxisMoving(), "percentage move requires manual reference");
  processCommand("JX1");
  advanceMs(100);
  processCommand("0");
  require(!isPositionReferenced(AXIS_X) && getCurrentPosition(AXIS_X) > 0, "cannot reset position while jogging");
  processCommand("J0");
  finishMove();
  processCommand("0X");
  require(isPositionReferenced(AXIS_X) && !isPositionReferenced(AXIS_Y), "per-axis zero references just that axis");
  processCommand("PX50");
  finishMove();
#if X_TRAVEL_STEPS > 0
  require(getCurrentPosition(AXIS_X) == X_TRAVEL_STEPS / 2, "percentage target uses configured travel");
  processCommand("JX1");
  advanceMs(2000);
  require(getCurrentPosition(AXIS_X) == X_TRAVEL_STEPS && !anyAxisMoving(), "jog brakes at upper software bound without overshoot");
  processCommand("JX-1");
  advanceMs(2000);
  require(getCurrentPosition(AXIS_X) == 0 && !anyAxisMoving(), "reverse jog brakes at lower software bound");
#else
  require(!anyAxisMoving(), "unknown travel prevents percentage movement");
  processCommand("X-3");
  finishMove();
  require(getCurrentPosition(AXIS_X) == -3, "no invented travel bounds");
#endif
  reset();
  setCurrentPosition(AXIS_X, LONG_MAX);
  processCommand("X1");
  require(!anyAxisMoving(), "counter overflow is refused");
  processCommand("JX1");
  advanceMs(100);
  require(getCurrentPosition(AXIS_X) == LONG_MAX, "jogging cannot overflow position counter");
  std::cout << "PASS: " << checks << " firmware checks (X travel=" << X_TRAVEL_STEPS << ")\n";
}
