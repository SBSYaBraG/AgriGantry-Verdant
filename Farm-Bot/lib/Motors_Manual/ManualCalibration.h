#ifndef MANUAL_CALIBRATION_H
#define MANUAL_CALIBRATION_H

#include "ManualConfig.h"

void manualCalibrationBegin();
void manualCalibrationUpdate();
bool manualCalibrationSelect(ManualAxis axis);
bool manualCalibrationStart(ManualAxis axis);
bool manualCalibrationJog(ManualAxis axis, int8_t direction);
bool manualCalibrationConfirmEndpoint();
bool manualCalibrationCancelEndpoint();
bool manualCalibrationMovePercent(ManualAxis axis, uint8_t percent);
bool manualCalibrationHome(ManualAxis axis);
ManualAxis manualCalibrationSelectedAxis();
bool manualCalibrationAxisReady(ManualAxis axis);
void manualCalibrationPrintStatus();

#endif
