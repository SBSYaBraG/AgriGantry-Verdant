#ifndef ALM_MONITOR_H
#define ALM_MONITOR_H
#include "Config.h"

void monitorAllALM();
void monitorMotorALM(Motor motor);
bool isAnyALMCritical();
bool isMotorALMCritical(Motor motor);
void printALMStatus();
const char* getMotorName(Motor motor);
#endif
