/** AF-434 hardware mapping with AgriGantry's three-axis command interface. */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

enum Axis { AXIS_NONE = 0, AXIS_X = 1, AXIS_Y = 2, AXIS_Z = 3 };
enum Motor { MOTOR_XL = 0, MOTOR_XR = 1, MOTOR_Y = 2, MOTOR_Z = 3, MOTOR_COUNT = 4 };

// Copied from AF-434/gantry_control/gantry_control.ino, commit 873e2b6.
// Follow the executable sketch: its X direction settings differ from its README.
#define XL_PUL_PIN 9
#define XL_DIR_PIN 8
#define XL_ENA_PIN 22
#define ALM_XL_PIN 26
#define XR_PUL_PIN 7
#define XR_DIR_PIN 6
#define XR_ENA_PIN 23
#define ALM_XR_PIN 27
#define Y_PUL_PIN 5
#define Y_DIR_PIN 4
#define Y_ENA_PIN 24
#define ALM_Y_PIN 28
#define Z_PUL_PIN 3
#define Z_DIR_PIN 2
#define Z_ENA_PIN 25
#define ALM_Z_PIN 29

#define ENA_ENABLED_LEVEL LOW
#define ENA_DISABLED_LEVEL HIGH
#define ALM_ACTIVE_LEVEL HIGH
#define ALM_DEBOUNCE_MS 5
#define CW_LEVEL HIGH
#define CCW_LEVEL LOW
#define XL_POSITIVE_IS_CW true
#define XR_POSITIVE_IS_CW true
#define Y_POSITIVE_IS_CW true
#define Z_POSITIVE_IS_CW false

struct MotorPins {
  Axis axis;
  uint8_t pul, dir, ena, alm;
  bool positiveIsCW;
};
static const MotorPins MOTOR_PINS[MOTOR_COUNT] = {
  {AXIS_X, XL_PUL_PIN, XL_DIR_PIN, XL_ENA_PIN, ALM_XL_PIN, XL_POSITIVE_IS_CW},
  {AXIS_X, XR_PUL_PIN, XR_DIR_PIN, XR_ENA_PIN, ALM_XR_PIN, XR_POSITIVE_IS_CW},
  {AXIS_Y, Y_PUL_PIN, Y_DIR_PIN, Y_ENA_PIN, ALM_Y_PIN, Y_POSITIVE_IS_CW},
  {AXIS_Z, Z_PUL_PIN, Z_DIR_PIN, Z_ENA_PIN, ALM_Z_PIN, Z_POSITIVE_IS_CW}
};

// No limit switches are installed. Set measured usable travel in pulses here
// to use percentage positioning and software bounds after a manual 0 command.
// Zero means unknown; do not invent a physical travel length.
#ifndef X_TRAVEL_STEPS
#define X_TRAVEL_STEPS 0L
#endif
#ifndef Y_TRAVEL_STEPS
#define Y_TRAVEL_STEPS 0L
#endif
#ifndef Z_TRAVEL_STEPS
#define Z_TRAVEL_STEPS 0L
#endif
static_assert(X_TRAVEL_STEPS >= 0 && Y_TRAVEL_STEPS >= 0 && Z_TRAVEL_STEPS >= 0,
              "Travel lengths must be nonnegative step counts");

// AF-434 velocity ramp and Timer1 step generator. Units are pulses and seconds.
#define STEP_TICK_HZ 20000U
#define STEP_PULSE_WIDTH_US 3
#define DEFAULT_JOG_SPEED 1000.0f
#define MIN_JOG_SPEED 100.0f
#define MAX_JOG_SPEED 8000.0f
#define JOG_SPEED_FACTOR 1.25f
#define MOTION_ACCELERATION 3000.0f
#define CRAWL_SPEED 150.0f
#define CONTROL_PERIOD_US 1000UL
#define JOG_TIMEOUT_MS 500UL
#define SERIAL_BAUD_RATE 115200
#define FIRMWARE_ID "AGRIGANTRY-KEYBOARD-v1"

#endif
