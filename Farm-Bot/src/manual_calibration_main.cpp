// Standalone safety-first firmware for manual calibration of the currently
// connected ICL drivers on Arturo's four-driver pin map. This source is only
// built by megaatmega2560-manual-calibration.
#include <Arduino.h>

#include "ManualCalibration.h"
#include "ManualConfig.h"
#include "ManualMotion.h"
#include "ManualProtocol.h"
#include "ManualSafety.h"

void setup() {
  Serial.begin(MANUAL_SERIAL_BAUD);
  manualMotionBegin();       // PUL low and all drivers disabled before anything can move.
  manualSafetyBegin();       // ALM inputs are checked continuously after setup.
  manualCalibrationBegin();  // No position is trusted after every power cycle.

  Serial.println();
  Serial.println(F("FarmBot manual calibration controller"));
  Serial.println(MANUAL_FIRMWARE_ID);
  Serial.println(F("Drivers are DISABLED. No automatic homing or startup motion."));
  Serial.println(F("Use the PC keyboard controller: select an axis, then press C to calibrate."));
  manualProtocolBegin();
}

void loop() {
  manualSafetyPoll();
  manualProtocolPoll();
  manualCalibrationUpdate();
  manualMotionUpdate();
}
