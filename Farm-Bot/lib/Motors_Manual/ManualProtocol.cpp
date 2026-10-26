#include "ManualProtocol.h"

#include "ManualCalibration.h"
#include "ManualConfig.h"
#include "ManualMotion.h"
#include "ManualSafety.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace {

char inputBuffer[64] = {};
uint8_t inputLength = 0;
bool inputOverflow = false;

void uppercase(char* text) {
  for (; *text; ++text) *text = static_cast<char>(toupper(static_cast<unsigned char>(*text)));
}

ManualAxis parseAxisAfter(const char* text, size_t offset) {
  while (text[offset] == ' ') ++offset;
  return manualAxisFromChar(text[offset]);
}

bool parsePercent(const char* text, long& value) {
  char* end = nullptr;
  value = strtol(text, &end, 10);
  return end != text && *end == '\0';
}

void processCommand(char* command) {
  uppercase(command);
  if (command[0] == '\0') return;

  if (strcmp(command, "?") == 0) {
    Serial.println(MANUAL_FIRMWARE_ID);
  } else if (strcmp(command, "HB") == 0) {
    manualMotionNoteHeartbeat();
  } else if (strcmp(command, "O") == 0 || strcmp(command, "!") == 0 ||
             strcmp(command, "ESTOP") == 0) {
    manualSafetyEmergencyStop(F("SOFTWARE"));
  } else if (strcmp(command, "CLEAR") == 0) {
    manualSafetyClearFault();
  } else if (strcmp(command, "STOP") == 0 || strcmp(command, "J0") == 0) {
    manualMotionControlledStop();
  } else if (strcmp(command, "CONFIRM") == 0) {
    manualCalibrationConfirmEndpoint();
  } else if (strcmp(command, "CANCEL") == 0) {
    manualCalibrationCancelEndpoint();
  } else if (strcmp(command, "STATUS") == 0 || strcmp(command, "R") == 0) {
    manualMotionPrintStatus();
    manualCalibrationPrintStatus();
    manualSafetyPrintStatus();
  } else if (strcmp(command, "ALM") == 0) {
    manualSafetyPrintStatus();
  } else if (strcmp(command, "HELP") == 0) {
    manualProtocolPrintHelp();
  } else if (strncmp(command, "SELECT", 6) == 0) {
    manualCalibrationSelect(parseAxisAfter(command, 6));
  } else if (strncmp(command, "CAL", 3) == 0) {
    manualCalibrationStart(parseAxisAfter(command, 3));
  } else if (strncmp(command, "HOME", 4) == 0) {
    manualCalibrationHome(parseAxisAfter(command, 4));
  } else if (command[0] == 'J' && manualIsAxis(manualAxisFromChar(command[1]))) {
    long direction = 0;
    if (parsePercent(command + 2, direction) && direction >= -1 && direction <= 1) {
      manualCalibrationJog(manualAxisFromChar(command[1]), static_cast<int8_t>(direction));
    } else {
      Serial.println(F("ERR: jog format is JX-1, JX0, or JX1"));
    }
  } else if (command[0] == 'P' && manualIsAxis(manualAxisFromChar(command[1]))) {
    long percent = 0;
    if (parsePercent(command + 2, percent) && percent >= 0 && percent <= 100) {
      manualCalibrationMovePercent(manualAxisFromChar(command[1]), static_cast<uint8_t>(percent));
    } else {
      Serial.println(F("ERR: percentage format is PX10, PX25, PX50, PX75, or PX90"));
    }
  } else {
    Serial.println(F("ERR: unknown command; send HELP"));
  }
}

}  // namespace

void manualProtocolBegin() { manualProtocolPrintHelp(); }

void manualProtocolPrintHelp() {
  Serial.println(F("MANUAL CALIBRATION COMMANDS"));
  Serial.println(F("CAL X|Y|Z: begin manual endpoint calibration"));
  Serial.println(F("JX-1/JX1, JY-1/JY1, JZ-1/JZ1: hold-to-jog during calibration"));
  Serial.println(F("JX0/JY0/JZ0 or STOP: controlled stop"));
  Serial.println(F("CONFIRM / CANCEL: accept or reject the 2-second endpoint prompt"));
  Serial.println(F("PX10/PX25/PX50/PX75/PX90: move a calibrated axis"));
  Serial.println(F("HOME X|Y|Z: move to stored 0%, not physical auto-homing"));
  Serial.println(F("O or ESTOP: immediate latched stop; CLEAR only after ALM is normal"));
  Serial.println(F("STATUS, ALM, ?, HELP"));
}

void manualProtocolPoll() {
  for (uint8_t count = 0; count < 32 && Serial.available(); ++count) {
    const char incoming = static_cast<char>(Serial.read());
    // Permit an immediate emergency character, but only at the start of a
    // packet so the O in HOME cannot be mistaken for an emergency stop.
    if ((incoming == '!' || incoming == 'O' || incoming == 'o') && inputLength == 0) {
      inputOverflow = false;
      manualSafetyEmergencyStop(F("SOFTWARE"));
      continue;
    }
    if (incoming == '\n' || incoming == '\r') {
      if (inputOverflow) {
        inputOverflow = false;
        inputLength = 0;
        Serial.println(F("ERR: command too long"));
      } else if (inputLength > 0) {
        inputBuffer[inputLength] = '\0';
        processCommand(inputBuffer);
        inputLength = 0;
      }
    } else if (!inputOverflow) {
      if (inputLength < sizeof(inputBuffer) - 1) {
        inputBuffer[inputLength++] = incoming;
      } else {
        inputOverflow = true;
      }
    }
  }
}
