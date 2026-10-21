#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H
#include "Config.h"

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
