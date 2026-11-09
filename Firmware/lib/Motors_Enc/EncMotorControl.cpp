/**
 * EncMotorControl.cpp - TB6600 pulse generation implementation
 */

#include "EncMotorControl.h"
#include "EncConfig.h"
#include "EncPositionManager.h"

static void setTestDirection(bool forward) {
  const bool level = forward ? FORWARD_DIR_LEVEL : !FORWARD_DIR_LEVEL;
  digitalWrite(TB6600_DIR_PIN, level);
  delayMicroseconds(DIRECTION_SETUP_US);
}

// The bench TB6600 uses ENA as an active-low offline input. This mirrors the
// tested Motors_ICL behavior: disable/free the shaft at idle, energize it only
// immediately before a commanded move, then disable it again afterwards.
static void enableTestMotor() {
  digitalWrite(TB6600_ENA_PIN, LOW);
  delay(ENABLE_STABILIZATION_MS);
}

static void disableTestMotor() {
  digitalWrite(TB6600_ENA_PIN, HIGH);
}

static void sendStepPulse(unsigned long periodUs) {
  digitalWrite(TB6600_PUL_PIN, HIGH);
  delayMicroseconds(PULSE_HIGH_US);
  digitalWrite(TB6600_PUL_PIN, LOW);

  const unsigned long lowTimeUs = periodUs > PULSE_HIGH_US ? periodUs - PULSE_HIGH_US : 1;
  delayMicroseconds(lowTimeUs);
}

static unsigned long periodForPps(unsigned int pulsesPerSecond) {
  return 1000000UL / pulsesPerSecond;
}

static EncMoveResult createResult(long pulses, bool forward, EncMoveType type, unsigned int targetPps) {
  EncMoveResult result;
  result.commandedPulses = pulses;
  result.encoderBefore = getEncoderCount();
  result.indexBefore = getEncoderIndexCount();
  result.durationMs = millis();
  result.targetPps = targetPps;
  result.forward = forward;
  result.type = type;
  return result;
}

static void finishResult(EncMoveResult& result) {
  result.durationMs = millis() - result.durationMs;
  result.encoderAfter = getEncoderCount();
  result.indexAfter = getEncoderIndexCount();
}

void initializeEncMotorControl() {
  pinMode(TB6600_PUL_PIN, OUTPUT);
  pinMode(TB6600_DIR_PIN, OUTPUT);
  pinMode(TB6600_ENA_PIN, OUTPUT);

  digitalWrite(TB6600_PUL_PIN, LOW);
  digitalWrite(TB6600_DIR_PIN, !FORWARD_DIR_LEVEL);
  disableTestMotor();
}

EncMoveResult moveEncMotorConstant(long pulses, bool forward) {
  setTestDirection(forward);
  enableTestMotor();
  beginEncoderIndexEventCapture();
  EncMoveResult result = createResult(pulses, forward, EncMoveType::ConstantSpeed, CONSTANT_TEST_PPS);

  const unsigned long periodUs = periodForPps(CONSTANT_TEST_PPS);
  for (long step = 0; step < pulses; step++) {
    sendStepPulse(periodUs);
  }

  finishResult(result);
  endEncoderIndexEventCapture();
  disableTestMotor();
  return result;
}

EncMoveResult moveEncMotorRamp(long pulses, bool forward, unsigned int targetPps) {
  if (targetPps < RAMP_START_PPS) targetPps = RAMP_START_PPS;
  if (targetPps > RAMP_MAX_PPS) targetPps = RAMP_MAX_PPS;

  setTestDirection(forward);
  enableTestMotor();
  beginEncoderIndexEventCapture();
  EncMoveResult result = createResult(pulses, forward, EncMoveType::Ramp, targetPps);

  const long rampSteps = min(pulses / 4, 200L);
  for (long step = 0; step < pulses; step++) {
    unsigned int currentPps = targetPps;

    if (rampSteps > 0 && step < rampSteps) {
      currentPps = RAMP_START_PPS + ((targetPps - RAMP_START_PPS) * step) / rampSteps;
    } else if (rampSteps > 0 && step >= pulses - rampSteps) {
      const long remaining = pulses - step - 1;
      currentPps = RAMP_START_PPS + ((targetPps - RAMP_START_PPS) * remaining) / rampSteps;
    }

    sendStepPulse(periodForPps(currentPps));
  }

  finishResult(result);
  endEncoderIndexEventCapture();
  disableTestMotor();
  return result;
}
