#include "CommandProcessor.h"
#include "Config.h"
#include "MotorControl.h"
#include "PositionManager.h"
#include "SystemOperations.h"
#include "ALM_Monitor.h"
#include <stdlib.h>
#include <errno.h>

static char inputBuffer[48];
static uint8_t inputLength = 0;
static bool inputOverflow = false;
static bool discardLine = false;

static Axis axisFromChar(char c) {
  return c == 'X' ? AXIS_X : c == 'Y' ? AXIS_Y : c == 'Z' ? AXIS_Z : AXIS_NONE;
}

static bool parseNumber(const String& text, long& value) {
  if (text.length() == 0) return false;
  char* end = 0;
  errno = 0;
  value = strtol(text.c_str(), &end, 10);
  return errno != ERANGE && end != text.c_str() && *end == '\0';
}

void processCommand(String command) {
  command.trim();
  command.toUpperCase();
  if (command.length() == 0) return;

  if (command == "?") {
    Serial.println(F(FIRMWARE_ID));
  } else if (command == "HB") {
    noteJogHeartbeat();
  } else if (command == "J0") {
    smoothStopAll();
  } else if (command == "J+" || command == "J-") {
    changeJogSpeed(command[1] == '+');
  } else if (command.length() >= 3 && command[0] == 'J' && axisFromChar(command[1]) != AXIS_NONE) {
    long direction;
    if (parseNumber(command.substring(2), direction) && direction >= -1 && direction <= 1) {
      jogAxis(axisFromChar(command[1]), (int8_t)direction);
    } else {
      Serial.println(F("Invalid jog direction; use -1, 0, or 1"));
    }
  } else if (command == "O" || command == "S" || command == "STOP" || command == "!") {
    emergencyStop();
  } else if (command == "S0" || command == "CLEAR") {
    resumeOperations();
  } else if (command == "R" || command == "STATUS") {
    reportStatus();
  } else if (command == "ALM") {
    printALMStatus();
  } else if (command == "0") {
    zeroPosition();
  } else if (command.length() == 2 && command[0] == '0' && axisFromChar(command[1]) != AXIS_NONE) {
    zeroPosition(axisFromChar(command[1]));
  } else if (command == "H" || command == "HALL" || command == "HX" || command == "HY" || command == "HZ") {
    runHomingAll();
  } else if (axisFromChar(command[0]) != AXIS_NONE) {
    long steps;
    if (parseNumber(command.substring(1), steps)) {
      processRelativeMove(axisFromChar(command[0]), steps);
    } else {
      Serial.println(F("Invalid step count. Example: X1000 or Y-500"));
    }
  } else if (command.length() >= 3 && command[0] == 'P' && axisFromChar(command[1]) != AXIS_NONE) {
    long percent;
    if (parseNumber(command.substring(2), percent) && (percent == 25 || percent == 50 || percent == 75)) {
      processAbsoluteMove(axisFromChar(command[1]), (int)percent);
    } else {
      Serial.println(F("Invalid percentage. Example: PX25, PY50, or PZ75"));
    }
  } else if (command == "HELP") {
    Serial.println(F("X1000 / Y-500 / Z100: relative axis steps; one axis at a time"));
    Serial.println(F("PX25 / PY50 / PZ75: percentage of configured travel after manual 0"));
    Serial.println(F("0: record all current positions as the origin; 0X / 0Y / 0Z: one axis"));
    Serial.println(F("O: emergency stop ALL motors immediately, no newline needed"));
    Serial.println(F("Keyboard: Pi/gantry_keyboard.py; W/S X, A/D Y, Q/E Z, Space smooth stop"));
    Serial.println(F("S / STOP / ! also stop; CLEAR / S0: clear after fixing faults"));
    Serial.println(F("STATUS / R: position, reference, travel; ALM: driver alarms; HELP: commands"));
    Serial.println(F("No limit switches or automatic homing. H/HX/HY/HZ/HALL do not move."));
    Serial.println(F("Commands end with newline; O and ! stop without newline. O is reserved."));
  } else {
    Serial.println(F("Unknown command. Type HELP"));
  }
}

void reportStatus() {
  Serial.println(F("AgriGantry / AF-434 wiring: four motors, no limit switches"));
  for (uint8_t i = AXIS_X; i <= AXIS_Z; ++i) {
    Axis axis = (Axis)i;
    Serial.print((char)('X' + i - AXIS_X));
    Serial.print(F(": position="));
    Serial.print(getCurrentPosition(axis));
    Serial.print(F(" | velocity="));
    Serial.print(getAxisVelocity(axis));
    Serial.print(isPositionReferenced(axis) ? F(" | manual zero set") : F(" | unreferenced"));
    Serial.print(F(" | travel="));
    Serial.print(getMaxPosition(axis));
    if (isPositionReferenced(axis) && getMaxPosition(axis) > 0) {
      Serial.print(F(" | percent="));
      Serial.print(getPositionPercentage(axis));
    }
    Serial.println();
  }
  Serial.println(isEmergencyStopActive() ? F("EMERGENCY STOP active") : F("Ready"));
  Serial.print(F("Jog speed="));
  Serial.println((long)getJogSpeed());
}

static void consumeSerial(bool allowMotionCommands) {
  // Shared nonblocking parser: a partial line must never pause motor pulses.
  // Clear the buffer BEFORE dispatch so checks during a move can parse STOP.
  for (uint8_t i = 0; i < 32 && Serial.available(); ++i) {
    char c = (char)Serial.read();
    if (c == '!' || c == 'O' || c == 'o') {
      discardLine = discardLine || inputLength > 0 || inputOverflow;
      inputLength = 0;
      inputOverflow = false;
      emergencyStop();
    } else if (c == '\n' || c == '\r') {
      if (discardLine) {
        discardLine = false;
        inputLength = 0;
        inputOverflow = false;
        continue;
      }
      if (inputOverflow) {
        inputLength = 0;
        inputOverflow = false;
        Serial.println(F("Command too long; discarded"));
      } else if (inputLength) {
        inputBuffer[inputLength] = '\0';
        String command(inputBuffer);
        command.trim();
        command.toUpperCase();
        inputLength = 0;
        if (allowMotionCommands || command == "S" || command == "STOP" || command == "O") {
          processCommand(command);
        } else {
          Serial.println(F("Busy moving; press O to stop"));
        }
      }
    } else if (!inputOverflow && !discardLine) {
      if (inputLength < sizeof(inputBuffer) - 1) inputBuffer[inputLength++] = c;
      else inputOverflow = true;
    }
  }
}

void pollSerialCommands() {
  consumeSerial(true);
}
bool checkForEmergencyStop() {
  consumeSerial(false);
  return isEmergencyStopActive();
}
