#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H
#include <Arduino.h>

void processCommand(String command);
void pollSerialCommands();
void reportStatus();
bool checkForEmergencyStop();
#endif
