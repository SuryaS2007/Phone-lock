#pragma once

namespace HardwareConfig {
// Keep false until the unloaded servo calibration in HARDWARE_TRAINING.md is complete.
constexpr bool SERVO_ENABLED = false;
constexpr bool SERVO_CALIBRATION_MODE = false;
// One-shot bench test: sweep 90 -> 0 -> 90 three times, then detach.
// Never enable while the servo is mechanically constrained.
constexpr bool SERVO_CYCLE_TEST_MODE = false;
constexpr int SERVO_CYCLE_ORIGINAL_ANGLE = 90;
constexpr int SERVO_CYCLE_CCW_ANGLE = 0;
constexpr unsigned int SERVO_CYCLE_REPETITIONS = 3;
constexpr unsigned int SERVO_CYCLE_STEP_DELAY_MS = 15;
constexpr unsigned int SERVO_CYCLE_HOLD_MS = 500;
// Direct-position test using the team's physical labels:
// physical 270 -> 180 -> 270 maps to servo commands 90 -> 0 -> 90.
constexpr bool SERVO_SWIFT_TEST_MODE = false;
constexpr int SERVO_SWIFT_270_COMMAND = 90;
constexpr int SERVO_SWIFT_180_COMMAND = 0;
constexpr unsigned int SERVO_SWIFT_HOLD_MS = 700;
constexpr int SERVO_CALIBRATION_START_ANGLE = 90;
constexpr int SERVO_UNLOCK_ANGLE = 30; // Placeholder: replace after calibration.
constexpr int SERVO_LOCK_ANGLE = 90;   // Placeholder: replace after calibration.
constexpr int SERVO_MIN_PULSE_US = 500;
constexpr int SERVO_MAX_PULSE_US = 2400;

// Local-network calibration page. Disable this before deployment if desired.
constexpr bool DEVELOPER_TOOLS_ENABLED = true;
constexpr int SERVO_WEB_MIN_ANGLE = 0;
constexpr int SERVO_WEB_MAX_ANGLE = 180;
constexpr unsigned int SERVO_WEB_MAX_REPETITIONS = 10;
constexpr unsigned int SERVO_WEB_MIN_HOLD_MS = 100;
constexpr unsigned int SERVO_WEB_MAX_HOLD_MS = 5000;

// The claws are fixed. Each sensor measures whether the pencil is resting above it.
// Replace these placeholder thresholds using the training readings.
constexpr bool PENCIL_PRESENT_WHEN_DISTANCE_LESS = true;
constexpr unsigned int LEFT_PENCIL_THRESHOLD_MM = 80;
constexpr unsigned int RIGHT_PENCIL_THRESHOLD_MM = 80;
constexpr unsigned long PENCIL_DEBOUNCE_MS = 350;
constexpr unsigned int SENSOR_TIMEOUT_MS = 100;
constexpr unsigned int SENSOR_PERIOD_MS = 50;
constexpr bool SENSOR_DIAGNOSTICS = true;

// Demo fallback: RFID is bypassed and the two shells are selected on the website.
constexpr bool RFID_ENABLED = false;
constexpr char MANUAL_SHELL_1_UID[] = "SHELL1";
constexpr char MANUAL_SHELL_2_UID[] = "SHELL2";
constexpr unsigned long MANUAL_SHELL_1_DEFAULT_SECONDS = 15UL * 60UL;
constexpr unsigned long MANUAL_SHELL_2_DEFAULT_SECONDS = 30UL * 60UL;

constexpr char ACCESS_POINT_NAME[] = "Focus-Lock";
constexpr unsigned long MAX_SESSION_SECONDS = 8UL * 60UL * 60UL;
constexpr unsigned long DEFAULT_SHELL_SECONDS = 25UL * 60UL;
constexpr unsigned int MAX_SHELLS = 12;

constexpr bool TM1637_ENABLED = true;
constexpr unsigned char TM1637_BRIGHTNESS = 5;

// Common I2C-backpack address. Change to 0x3F if an I2C scan finds that address.
constexpr bool LCD1602_ENABLED = true;
constexpr unsigned char LCD1602_ADDRESS = 0x27;

// Assumes a passive buzzer. Set false until the buzzer type and wiring are verified.
constexpr bool BUZZER_ENABLED = false;
constexpr bool BUZZER_TEST_MODE = false;
constexpr unsigned char BUZZER_PWM_CHANNEL = 7;

// The second SG90 sways the crab only while actively studying.
constexpr bool SHAKE_SERVO_ENABLED = false;
constexpr int SHAKE_CENTER_ANGLE = 90;
constexpr int SHAKE_LEFT_ANGLE = 75;
constexpr int SHAKE_RIGHT_ANGLE = 105;
constexpr unsigned long SHAKE_INTERVAL_MS = 700;
} // namespace HardwareConfig
