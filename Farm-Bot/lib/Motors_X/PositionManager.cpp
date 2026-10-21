#include "PositionManager.h"
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
