#ifndef POSITION_MANAGER_H
#define POSITION_MANAGER_H
#include "Config.h"

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
