#ifndef SYSTEM_OPERATIONS_H
#define SYSTEM_OPERATIONS_H
#include "Config.h"

void runHoming(Axis axis);
void runHomingAll();
void zeroPosition(Axis axis = AXIS_NONE);
#endif
