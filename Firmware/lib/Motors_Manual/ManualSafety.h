#ifndef MANUAL_SAFETY_H
#define MANUAL_SAFETY_H

#include "ManualConfig.h"

void manualSafetyBegin();
void manualSafetyPoll();
bool manualSafetyHasFault();
void manualSafetyEmergencyStop(const __FlashStringHelper* reason);
bool manualSafetyClearFault();
void manualSafetyPrintStatus();

#endif
