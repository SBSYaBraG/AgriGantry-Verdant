#ifndef MANUAL_CONFIG_H
#define MANUAL_CONFIG_H

#include <Arduino.h>

// This firmware intentionally uses the four-driver wiring from Arturo's
// AF-434 controller. It does not use the incompatible Motors_ICL pin map.
enum class ManualAxis : uint8_t { NONE = 0, X = 1, Y = 2, Z = 3 };
enum class ManualMotor : uint8_t { X_LEFT = 0, X_RIGHT = 1, Y = 2, Z = 3, COUNT = 4 };

struct ManualMotorPins {
  ManualAxis axis;
  uint8_t pul;
  uint8_t dir;
  uint8_t ena;
  uint8_t alm;
  // Match the existing Motors_ICL convention: a positive logical direction
  // is a HIGH DIR level. Physical left/right is confirmed during first jog.
  bool positiveDirectionHigh;
};

static const ManualMotorPins MANUAL_MOTOR_PINS[] = {
    {ManualAxis::X, 9, 8, 22, 26, true},
    {ManualAxis::X, 7, 6, 23, 27, true},
    {ManualAxis::Y, 5, 4, 24, 28, true},
    {ManualAxis::Z, 3, 2, 25, 29, true},
};

constexpr uint8_t MANUAL_MOTOR_COUNT = static_cast<uint8_t>(ManualMotor::COUNT);
constexpr unsigned long MANUAL_SERIAL_BAUD = 115200UL;
constexpr char MANUAL_FIRMWARE_ID[] = "FARMBOT-MANUAL-CAL-v1";

constexpr uint8_t MANUAL_ENA_ENABLED_LEVEL = LOW;
constexpr uint8_t MANUAL_ENA_DISABLED_LEVEL = HIGH;
constexpr uint8_t MANUAL_ALM_ACTIVE_LEVEL = HIGH;
constexpr uint8_t MANUAL_ALM_DEBOUNCE_MS = 5;
constexpr uint16_t MANUAL_ENABLE_SETTLE_MS = 200;

constexpr uint16_t MANUAL_STEP_TICK_HZ = 20000;
constexpr uint8_t MANUAL_STEP_PULSE_US = 4;
constexpr uint16_t MANUAL_CONTROL_PERIOD_US = 1000;
constexpr uint16_t MANUAL_JOG_HEARTBEAT_MS = 350;
constexpr uint16_t MANUAL_ENDPOINT_IDLE_MS = 2000;
constexpr long MANUAL_MIN_CALIBRATION_STEPS = 100;

// Deliberately conservative axis-specific cruise speeds. Jogging during
// calibration uses the lower set; percentage moves (and the midpoint return)
// use the normal set. Acceleration/deceleration remains ramped below so every
// pulse during a release/stop is still commanded and counted.
constexpr float MANUAL_CALIBRATION_X_CRUISE_PPS = 2500.0f;
constexpr float MANUAL_CALIBRATION_Y_CRUISE_PPS = 2000.0f;
constexpr float MANUAL_CALIBRATION_Z_CRUISE_PPS = 1500.0f;
constexpr float MANUAL_CALIBRATION_ACCEL_PPS2 = 2200.0f;
constexpr float MANUAL_CALIBRATION_DECEL_PPS2 = 2500.0f;
constexpr float MANUAL_NORMAL_X_CRUISE_PPS = 3000.0f;
constexpr float MANUAL_NORMAL_Y_CRUISE_PPS = 2500.0f;
constexpr float MANUAL_NORMAL_Z_CRUISE_PPS = 1800.0f;
constexpr float MANUAL_NORMAL_ACCEL_PPS2 = 10000.0f;
constexpr float MANUAL_NORMAL_DECEL_PPS2 = 12000.0f;
constexpr float MANUAL_MIN_TARGET_PPS = 150.0f;

inline bool manualIsAxis(ManualAxis axis) {
  return axis == ManualAxis::X || axis == ManualAxis::Y || axis == ManualAxis::Z;
}

inline char manualAxisName(ManualAxis axis) {
  switch (axis) {
    case ManualAxis::X: return 'X';
    case ManualAxis::Y: return 'Y';
    case ManualAxis::Z: return 'Z';
    default: return '-';
  }
}

inline ManualAxis manualAxisFromChar(char value) {
  return value == 'X' ? ManualAxis::X : value == 'Y' ? ManualAxis::Y
      : value == 'Z' ? ManualAxis::Z : ManualAxis::NONE;
}

inline float manualCalibrationCruisePps(ManualAxis axis) {
  switch (axis) {
    case ManualAxis::X: return MANUAL_CALIBRATION_X_CRUISE_PPS;
    case ManualAxis::Y: return MANUAL_CALIBRATION_Y_CRUISE_PPS;
    case ManualAxis::Z: return MANUAL_CALIBRATION_Z_CRUISE_PPS;
    default: return 0.0f;
  }
}

inline float manualNormalCruisePps(ManualAxis axis) {
  switch (axis) {
    case ManualAxis::X: return MANUAL_NORMAL_X_CRUISE_PPS;
    case ManualAxis::Y: return MANUAL_NORMAL_Y_CRUISE_PPS;
    case ManualAxis::Z: return MANUAL_NORMAL_Z_CRUISE_PPS;
    default: return 0.0f;
  }
}

#endif
