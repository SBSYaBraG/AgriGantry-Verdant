#ifndef MANUAL_MOTION_H
#define MANUAL_MOTION_H

#include "ManualConfig.h"

// Call manualMotionUpdate() frequently from loop() and manualMotionTick() from
// Timer1. Only one gantry axis may be active at a time in this safety mode.
void manualMotionBegin();
void manualMotionUpdate();
void manualMotionTick();

bool manualMotionRequestJog(ManualAxis axis, int8_t direction);
bool manualMotionRequestTarget(ManualAxis axis, long rawTargetPosition);
void manualMotionNoteHeartbeat();
void manualMotionControlledStop();
void manualMotionEmergencyStop();
void manualMotionClearEmergency();

bool manualMotionIsIdle();
bool manualMotionEmergencyActive();
// Valid after a target move has become idle. False means it was stopped or
// interrupted before reaching its requested coordinate.
bool manualMotionLastTargetReached();
ManualAxis manualMotionActiveAxis();
long manualMotionGetPosition(ManualAxis axis);
void manualMotionSetPosition(ManualAxis axis, long position);
int16_t manualMotionGetVelocity();
void manualMotionPrintStatus();

#endif
