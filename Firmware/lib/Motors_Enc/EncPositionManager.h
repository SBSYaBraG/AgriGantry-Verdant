/**
 * EncPositionManager.h - Quadrature encoder measurement for the bench test
 */

#ifndef ENC_POSITION_MANAGER_H
#define ENC_POSITION_MANAGER_H

#include <Arduino.h>

void initializeEncPositionManager();
long getEncoderCount();
long getEncoderIndexCount();
void resetEncoderMeasurements();
void resetEncoderIndexEvents();
void beginEncoderIndexEventCapture();
void endEncoderIndexEventCapture();
void printEncoderIndexEvents();
uint8_t getEncoderState();
bool getEncoderIndexState();

#endif  // ENC_POSITION_MANAGER_H
