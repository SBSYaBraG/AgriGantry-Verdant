/** AgriGantry commands on AF-434's working four-motor Mega 2560 wiring. */
#include "Config.h"
#include "MotorControl.h"
#include "CommandProcessor.h"
#include "ALM_Monitor.h"

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  initializeMotor();
  Serial.println(F("AgriGantry controller - AF-434 hardware"));
  Serial.println(F("No limit switches: no startup motion or automatic homing."));
  Serial.println(F("Relative commands: X1000, Y-500, Z100. Send HELP for all commands."));
  Serial.println(F("Press O in the Python controller to stop all motors immediately."));
  Serial.println(F("Keyboard jogging: run Pi/gantry_keyboard.py; release keys to ramp to a stop."));
  Serial.println(F("Percentage moves need measured travel settings and a manual 0."));
}

void loop() {
  monitorAllALM();
  pollSerialCommands();
  updateMotorControl();
}
