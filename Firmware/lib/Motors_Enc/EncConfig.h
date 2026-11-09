/**
 * EncConfig.h - Encoder-motor bench-test configuration
 *
 * This configuration is only for the 17HS19-2004-ME1K and TB6600 bench test.
 */

#ifndef ENC_CONFIG_H
#define ENC_CONFIG_H

#include <Arduino.h>

// TB6600 common-cathode command wiring for the differential-receiver bench
// test. PUL-, DIR-, and ENA- are tied to Arduino GND; D4/D5/D6 drive the
// corresponding positive terminals. This TB6600's ENA input is active LOW:
// LOW enables motor outputs and HIGH takes the motor offline.
constexpr uint8_t TB6600_PUL_PIN = 4;
constexpr uint8_t TB6600_DIR_PIN = 5;
constexpr uint8_t TB6600_ENA_PIN = 6;

// AM26LV32E single-ended receiver outputs.  These are Mega interrupt pins.
constexpr uint8_t ENCODER_A_PIN = 2;   // AM26 pin 3 (1Y), derived from A+/A-
constexpr uint8_t ENCODER_B_PIN = 3;   // AM26 pin 5 (2Y), derived from B+/B-
constexpr uint8_t ENCODER_Z_PIN = 18;  // AM26 pin 11 (3Y), derived from Z+/Z-

// Motor and driver settings
constexpr long ENCODER_COUNTS_PER_REV = 4000L;
// Current TB6600 DIP-switch setting: 1600 STEP pulses make one shaft
// revolution. Keep this value synchronized with the physical driver setting.
constexpr long TB6600_PULSES_PER_REV = 1600L;
constexpr bool FORWARD_DIR_LEVEL = HIGH;

// TB6600 timing requirements are exceeded deliberately for a reliable bench test.
constexpr unsigned int PULSE_HIGH_US = 10;
constexpr unsigned int DIRECTION_SETUP_US = 10;
constexpr unsigned int ENABLE_STABILIZATION_MS = 200;
constexpr unsigned int CONSTANT_TEST_PPS = 100;
constexpr unsigned int RAMP_START_PPS = 100;
constexpr unsigned int RAMP_MAX_PPS = 600;

// A Z event is recorded for every rising edge during a commanded move. The
// fixed buffer avoids unsafe and timing-disturbing Serial output from the ISR.
constexpr uint8_t MAX_Z_EVENTS_PER_MOVE = 64;

#endif  // ENC_CONFIG_H
