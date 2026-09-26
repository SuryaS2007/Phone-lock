#pragma once

namespace HardwareConfig {
// Keep false until the unloaded servo calibration in HARDWARE_TRAINING.md is complete.
constexpr bool SERVO_ENABLED = false;
constexpr bool SERVO_CALIBRATION_MODE = false;
constexpr int SERVO_CALIBRATION_START_ANGLE = 90;
constexpr int SERVO_UNLOCK_ANGLE = 30; // Placeholder: replace after calibration.
constexpr int SERVO_LOCK_ANGLE = 90;   // Placeholder: replace after calibration.
constexpr int SERVO_MIN_PULSE_US = 500;
constexpr int SERVO_MAX_PULSE_US = 2400;

// Placeholder thresholds. Replace using readings collected with diagnostics enabled.
constexpr bool CLAW_LIFTED_WHEN_DISTANCE_GREATER = true;
constexpr unsigned int LEFT_CLAW_THRESHOLD_MM = 80;
constexpr unsigned int RIGHT_CLAW_THRESHOLD_MM = 80;
constexpr unsigned long CLAW_DEBOUNCE_MS = 350;
constexpr unsigned int SENSOR_TIMEOUT_MS = 100;
constexpr unsigned int SENSOR_PERIOD_MS = 50;
constexpr bool SENSOR_DIAGNOSTICS = true;

constexpr char ACCESS_POINT_NAME[] = "Focus-Lock";
constexpr unsigned long MAX_SESSION_SECONDS = 8UL * 60UL * 60UL;
} // namespace HardwareConfig
