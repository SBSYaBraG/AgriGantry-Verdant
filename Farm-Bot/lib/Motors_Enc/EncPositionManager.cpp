/**
 * EncPositionManager.cpp - Quadrature encoder measurement implementation
 */

#include "EncPositionManager.h"
#include "EncConfig.h"

static volatile long encoderCount = 0;
static volatile long indexCount = 0;
static volatile uint8_t previousState = 0;

struct EncoderIndexEvent {
  unsigned long timestampUs;
  long encoderCount;
};

static volatile EncoderIndexEvent indexEvents[MAX_Z_EVENTS_PER_MOVE];
static volatile uint8_t indexEventCount = 0;
static volatile unsigned int droppedIndexEventCount = 0;
static volatile bool indexEventCaptureActive = false;

// Index = previous AB state (bits 3:2) followed by current AB state (bits 1:0).
// Invalid two-bit jumps are ignored instead of counted as a false step.
static const int8_t quadratureDelta[16] = {
  0, -1,  1,  0,
  1,  0,  0, -1,
 -1,  0,  0,  1,
  0,  1, -1,  0
};

static uint8_t readABState() {
  return (digitalRead(ENCODER_A_PIN) << 1) | digitalRead(ENCODER_B_PIN);
}

static void updateQuadratureCount() {
  const uint8_t currentState = readABState();
  encoderCount += quadratureDelta[(previousState << 2) | currentState];
  previousState = currentState;
}

static void handleEncoderAChange() {
  updateQuadratureCount();
}

static void handleEncoderBChange() {
  updateQuadratureCount();
}

static void handleEncoderIndexRise() {
  // Do not write Serial from an ISR. Record the edge and the nearest completed
  // A/B quadrature position instead; the command processor prints this buffer
  // after the commanded move has ended.
  const unsigned long timestampUs = micros();
  const long countAtIndex = encoderCount;
  indexCount++;

  if (indexEventCaptureActive) {
    const uint8_t eventSlot = indexEventCount;
    if (eventSlot < MAX_Z_EVENTS_PER_MOVE) {
      indexEvents[eventSlot].timestampUs = timestampUs;
      indexEvents[eventSlot].encoderCount = countAtIndex;
      indexEventCount = eventSlot + 1;
    } else {
      droppedIndexEventCount++;
    }
  }
}

void initializeEncPositionManager() {
  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  pinMode(ENCODER_Z_PIN, INPUT);

  previousState = readABState();
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), handleEncoderAChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), handleEncoderBChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_Z_PIN), handleEncoderIndexRise, RISING);
}

long getEncoderCount() {
  noInterrupts();
  const long count = encoderCount;
  interrupts();
  return count;
}

long getEncoderIndexCount() {
  noInterrupts();
  const long count = indexCount;
  interrupts();
  return count;
}

void resetEncoderMeasurements() {
  noInterrupts();
  encoderCount = 0;
  indexCount = 0;
  indexEventCount = 0;
  droppedIndexEventCount = 0;
  indexEventCaptureActive = false;
  previousState = readABState();
  interrupts();
}

void resetEncoderIndexEvents() {
  noInterrupts();
  indexEventCount = 0;
  droppedIndexEventCount = 0;
  interrupts();
}

void beginEncoderIndexEventCapture() {
  noInterrupts();
  indexEventCount = 0;
  droppedIndexEventCount = 0;
  indexEventCaptureActive = true;
  interrupts();
}

void endEncoderIndexEventCapture() {
  noInterrupts();
  indexEventCaptureActive = false;
  interrupts();
}

void printEncoderIndexEvents() {
  noInterrupts();
  const uint8_t eventCountToPrint = indexEventCount;
  const unsigned int droppedEvents = droppedIndexEventCount;
  interrupts();

  for (uint8_t eventIndex = 0; eventIndex < eventCountToPrint; eventIndex++) {
    unsigned long timestampUs = 0;
    long countAtIndex = 0;

    noInterrupts();
    timestampUs = indexEvents[eventIndex].timestampUs;
    countAtIndex = indexEvents[eventIndex].encoderCount;
    interrupts();

    Serial.print("Z_EVENT,");
    Serial.print(eventIndex + 1);
    Serial.print(',');
    Serial.print(timestampUs);
    Serial.print(',');
    Serial.println(countAtIndex);
  }

  if (droppedEvents > 0) {
    Serial.print("Z_EVENT_OVERFLOW,");
    Serial.println(droppedEvents);
  }
}

uint8_t getEncoderState() {
  return readABState();
}

bool getEncoderIndexState() {
  return digitalRead(ENCODER_Z_PIN) == HIGH;
}
