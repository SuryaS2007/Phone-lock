#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <SPI.h>
#include <TM1637TinyDisplay.h>
#include <VL53L0X.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

#include "HardwareConfig.h"
#include "PinConfig.h"

enum class SessionState {
  WAITING_FOR_SHELL,
  ARMED,
  LOCKED_PAUSED,
  LOCKED_STUDYING,
  COMPLETE
};

struct ShellSetting {
  String uid;
  uint32_t durationSeconds;
};

class Lcd1602I2c {
 public:
  explicit Lcd1602I2c(uint8_t address) : address_(address) {}

  void begin() {
    delay(50);
    writeNibble(0x03, false); delay(5);
    writeNibble(0x03, false); delayMicroseconds(150);
    writeNibble(0x03, false);
    writeNibble(0x02, false);
    command(0x28); // 4-bit, 2 line, 5x8 font
    command(0x0C); // display on, cursor off
    command(0x06); // left-to-right entry
    clear();
  }

  void clear() { command(0x01); delay(2); }

  void printLine(uint8_t row, const String &text) {
    command(row == 0 ? 0x80 : 0xC0);
    String padded = text.substring(0, 16);
    while (padded.length() < 16) padded += ' ';
    for (char character : padded) send(static_cast<uint8_t>(character), true);
  }

  void createChar(uint8_t slot, const uint8_t glyph[8]) {
    command(static_cast<uint8_t>(0x40 | ((slot & 0x07) << 3)));
    for (uint8_t row = 0; row < 8; ++row) send(glyph[row], true);
  }

  void setCursor(uint8_t column, uint8_t row) {
    command(static_cast<uint8_t>((row == 0 ? 0x80 : 0xC0) + min(column, static_cast<uint8_t>(15))));
  }

  void writeChar(uint8_t character) { send(character, true); }

 private:
  static constexpr uint8_t BACKLIGHT = 0x08;
  static constexpr uint8_t ENABLE = 0x04;
  uint8_t address_;

  void command(uint8_t value) { send(value, false); }

  void send(uint8_t value, bool data) {
    writeNibble(value >> 4, data);
    writeNibble(value & 0x0F, data);
  }

  void writeNibble(uint8_t nibble, bool data) {
    const uint8_t output = static_cast<uint8_t>((nibble << 4) | BACKLIGHT | (data ? 0x01 : 0x00));
    Wire.beginTransmission(address_);
    Wire.write(output | ENABLE);
    Wire.endTransmission();
    delayMicroseconds(1);
    Wire.beginTransmission(address_);
    Wire.write(output & ~ENABLE);
    Wire.endTransmission();
    delayMicroseconds(50);
  }
};

enum class EyeExpression {
  CENTER,
  LEFT,
  RIGHT,
  BLINK,
  HAPPY,
  SURPRISED,
  SLEEP
};

// LCD1602 custom characters adapted from the teammate eye-expression prototype.
const uint8_t CENTER_TOP[8] = {
  0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101
};
const uint8_t CENTER_BOTTOM[8] = {
  0b10101, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110
};
const uint8_t LEFT_TOP[8] = {
  0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b11001, 0b11001, 0b11001
};
const uint8_t LEFT_BOTTOM[8] = {
  0b11001, 0b11001, 0b11001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110
};
const uint8_t RIGHT_TOP[8] = {
  0b01111, 0b10001, 0b10001, 0b10001, 0b10001, 0b10011, 0b10011, 0b10011
};
const uint8_t RIGHT_BOTTOM[8] = {
  0b10011, 0b10011, 0b10011, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110
};
const uint8_t BLINK_TOP[8] = {
  0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b00000
};
const uint8_t BLINK_BOTTOM[8] = {
  0b00000, 0b11111, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000
};
const uint8_t HAPPY_TOP[8] = {
  0b00000, 0b00000, 0b00000, 0b01110, 0b10001, 0b10001, 0b10001, 0b00000
};
const uint8_t HAPPY_BOTTOM[8] = {
  0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000
};
const uint8_t SURPRISED_TOP[8] = {
  0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10101, 0b10101
};
const uint8_t SURPRISED_BOTTOM[8] = {
  0b10101, 0b10101, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110
};
const uint8_t SLEEP_TOP[8] = {
  0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b10001, 0b01110, 0b00000
};
const uint8_t SLEEP_BOTTOM[8] = {
  0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000
};

WebServer server(80);
VL53L0X leftPencilSensor;
VL53L0X rightPencilSensor;
MFRC522 rfid(Pins::RFID_CS, Pins::RFID_RST);
TM1637TinyDisplay countdownDisplay(Pins::DISPLAY_CLK, Pins::DISPLAY_DIO);
Lcd1602I2c eyeDisplay(HardwareConfig::LCD1602_ADDRESS);
Servo lockServo;
Servo shakeServo;
Preferences preferences;
Preferences servoPreferences;

ShellSetting shells[HardwareConfig::MAX_SHELLS];
size_t shellCount = 0;
String selectedShellUid;
uint32_t selectedDurationSeconds = HardwareConfig::DEFAULT_SHELL_SECONDS;

bool sensorsReady = false;
bool leftSensorReady = false;
bool rightSensorReady = false;
bool rfidReady = false;
bool filesystemReady = false;
bool lockServoAttached = false;
bool shakeServoAttached = false;
bool drawerLocked = false;
int lastReportedApClients = -1;
int savedUnlockAngle = HardwareConfig::SERVO_UNLOCK_ANGLE;
int savedLockAngle = HardwareConfig::SERVO_LOCK_ANGLE;
int currentLockServoAngle = -1;

struct ServoSequenceState {
  bool active = false;
  int outerAngle = 90;
  int innerAngle = 0;
  unsigned int repetitions = 1;
  unsigned int completed = 0;
  unsigned int holdMs = 700;
  unsigned int leg = 0;
  unsigned long changedAt = 0;
};

ServoSequenceState servoSequence;

uint16_t leftDistanceMm = 0;
uint16_t rightDistanceMm = 0;
bool leftReadingValid = false;
bool rightReadingValid = false;
bool pencilRemoved = false;
bool pendingPencilRemoved = false;
bool pencilSeenAfterShellScan = false;
unsigned long pendingPencilChangeAt = 0;
unsigned long lastSensorReadAt = 0;
unsigned long lastDiagnosticAt = 0;
unsigned long lastRfidPollAt = 0;
unsigned long lastRfidDiagnosticAt = 0;
unsigned long lastDisplayUpdateAt = 0;
unsigned long lastShakeAt = 0;
bool shakeDirection = false;
EyeExpression currentEyeExpression = static_cast<EyeExpression>(255);
SessionState lastEyeSessionState = SessionState::COMPLETE;
unsigned long eyeAnimationStartedAt = 0;

SessionState sessionState = SessionState::WAITING_FOR_SHELL;
bool sessionActive = false;
uint32_t targetTimeMs = 0;
uint32_t accumulatedTimeMs = 0;
uint32_t studyingStartedAt = 0;

const char *stateName() {
  switch (sessionState) {
    case SessionState::WAITING_FOR_SHELL: return "WAITING_FOR_SHELL";
    case SessionState::ARMED: return "ARMED";
    case SessionState::LOCKED_PAUSED: return "LOCKED_PAUSED";
    case SessionState::LOCKED_STUDYING: return "LOCKED_STUDYING";
    case SessionState::COMPLETE: return "COMPLETE";
  }
  return "WAITING_FOR_SHELL";
}

uint32_t activeTimeMs() {
  uint32_t active = accumulatedTimeMs;
  if (sessionActive && sessionState == SessionState::LOCKED_STUDYING) active += millis() - studyingStartedAt;
  return targetTimeMs > 0 ? min(active, targetTimeMs) : active;
}

uint32_t remainingSeconds() {
  const uint32_t active = activeTimeMs();
  if (active >= targetTimeMs) return 0;
  return (targetTimeMs - active + 999UL) / 1000UL;
}

void playTone(uint16_t frequency, uint16_t durationMs) {
  if (!HardwareConfig::BUZZER_ENABLED && !HardwareConfig::BUZZER_TEST_MODE) return;
  ledcWriteTone(HardwareConfig::BUZZER_PWM_CHANNEL, frequency);
  delay(durationMs);
  ledcWriteTone(HardwareConfig::BUZZER_PWM_CHANNEL, 0);
}

void playStartSound() { playTone(880, 140); }

void playCompleteSound() {
  playTone(660, 120); delay(60);
  playTone(880, 120); delay(60);
  playTone(1100, 220);
}

void writeLockServo(int angle) {
  if (!lockServoAttached) return;
  currentLockServoAngle = constrain(angle, 0, 180);
  lockServo.write(currentLockServoAngle);
  delay(250);
}

bool attachLockServo() {
  if (lockServoAttached) return true;
  lockServo.setPeriodHertz(50);
  lockServo.attach(Pins::LOCK_SERVO, HardwareConfig::SERVO_MIN_PULSE_US, HardwareConfig::SERVO_MAX_PULSE_US);
  lockServoAttached = lockServo.attached();
  return lockServoAttached;
}

void detachLockServo() {
  servoSequence.active = false;
  if (lockServoAttached) lockServo.detach();
  lockServoAttached = false;
}

void unlockDrawer() {
  if (HardwareConfig::SERVO_ENABLED || HardwareConfig::SERVO_CALIBRATION_MODE) {
    if (attachLockServo()) writeLockServo(savedUnlockAngle);
  }
  drawerLocked = false;
}

void lockDrawer() {
  if (HardwareConfig::SERVO_ENABLED) {
    drawerLocked = attachLockServo();
    if (drawerLocked) writeLockServo(savedLockAngle);
  } else {
    drawerLocked = false;
    Serial.println("[SAFE MODE] Lock requested, but SERVO_ENABLED is false.");
  }
}

void stopShake() {
  if (shakeServoAttached) shakeServo.write(HardwareConfig::SHAKE_CENTER_ANGLE);
}

void updateShake() {
  if (!shakeServoAttached) return;
  if (sessionState != SessionState::LOCKED_STUDYING) {
    stopShake();
    return;
  }
  if (millis() - lastShakeAt < HardwareConfig::SHAKE_INTERVAL_MS) return;
  lastShakeAt = millis();
  shakeDirection = !shakeDirection;
  shakeServo.write(shakeDirection ? HardwareConfig::SHAKE_LEFT_ANGLE : HardwareConfig::SHAKE_RIGHT_ANGLE);
}

bool initializeSensors() {
  pinMode(Pins::LEFT_PENCIL_XSHUT, OUTPUT);
  pinMode(Pins::RIGHT_PENCIL_XSHUT, OUTPUT);
  digitalWrite(Pins::LEFT_PENCIL_XSHUT, LOW);
  digitalWrite(Pins::RIGHT_PENCIL_XSHUT, LOW);
  delay(20);

  digitalWrite(Pins::LEFT_PENCIL_XSHUT, HIGH);
  delay(20);
  leftSensorReady = leftPencilSensor.init();
  if (!leftSensorReady) {
    Serial.println("ERROR: Left pencil VL53L0X was not found.");
  } else {
    leftPencilSensor.setAddress(I2cAddresses::LEFT_PENCIL);
    leftPencilSensor.setTimeout(HardwareConfig::SENSOR_TIMEOUT_MS);
  }

  digitalWrite(Pins::RIGHT_PENCIL_XSHUT, HIGH);
  delay(20);
  rightSensorReady = rightPencilSensor.init();
  if (!rightSensorReady) {
    Serial.println("ERROR: Right pencil VL53L0X was not found.");
  } else {
    rightPencilSensor.setAddress(I2cAddresses::RIGHT_PENCIL);
    rightPencilSensor.setTimeout(HardwareConfig::SENSOR_TIMEOUT_MS);
  }

  if (leftSensorReady) leftPencilSensor.startContinuous(HardwareConfig::SENSOR_PERIOD_MS);
  if (rightSensorReady) rightPencilSensor.startContinuous(HardwareConfig::SENSOR_PERIOD_MS);

  if (!leftSensorReady && !rightSensorReady) {
    Serial.println("ERROR: No pencil VL53L0X sensors were detected.");
    return false;
  }

  if (!leftSensorReady || !rightSensorReady) {
    if (leftSensorReady) Serial.println("Detected only the LEFT pencil sensor through GPIO32 XSHUT.");
    if (rightSensorReady) Serial.println("Detected only the RIGHT pencil sensor through GPIO33 XSHUT.");
    Serial.println("DEMO MODE: The connected sensor will control pencil detection.");
    return true;
  }

  Serial.println("Both pencil sensors initialized at 0x30 and 0x31.");
  return true;
}

bool readingMeansPencilPresent(uint16_t distance, uint16_t threshold) {
  return HardwareConfig::PENCIL_PRESENT_WHEN_DISTANCE_LESS ? distance < threshold : distance > threshold;
}

void updatePencilSensors() {
  const unsigned long now = millis();
  if (!sensorsReady || now - lastSensorReadAt < HardwareConfig::SENSOR_PERIOD_MS) return;
  lastSensorReadAt = now;

  if (leftSensorReady) {
    leftDistanceMm = leftPencilSensor.readRangeContinuousMillimeters();
    leftReadingValid = !leftPencilSensor.timeoutOccurred() && leftDistanceMm < 8190;
  }
  if (rightSensorReady) {
    rightDistanceMm = rightPencilSensor.readRangeContinuousMillimeters();
    rightReadingValid = !rightPencilSensor.timeoutOccurred() && rightDistanceMm < 8190;
  }

  const bool connectedReadingsValid =
    (leftSensorReady || rightSensorReady) &&
    (!leftSensorReady || leftReadingValid) &&
    (!rightSensorReady || rightReadingValid);

  if (connectedReadingsValid) {
    const bool leftPresent = leftSensorReady && leftReadingValid &&
      readingMeansPencilPresent(leftDistanceMm, HardwareConfig::LEFT_PENCIL_THRESHOLD_MM);
    const bool rightPresent = rightSensorReady && rightReadingValid &&
      readingMeansPencilPresent(rightDistanceMm, HardwareConfig::RIGHT_PENCIL_THRESHOLD_MM);
    const bool rawRemoved = !leftPresent && !rightPresent;

    if (rawRemoved != pendingPencilRemoved) {
      pendingPencilRemoved = rawRemoved;
      pendingPencilChangeAt = now;
    } else if (pencilRemoved != pendingPencilRemoved && now - pendingPencilChangeAt >= HardwareConfig::PENCIL_DEBOUNCE_MS) {
      pencilRemoved = pendingPencilRemoved;
      Serial.printf("Pencil: %s\n", pencilRemoved ? "REMOVED / STUDYING" : "RESTING / PAUSED");
    }
  }

  if (HardwareConfig::SENSOR_DIAGNOSTICS && now - lastDiagnosticAt >= 500) {
    lastDiagnosticAt = now;
    Serial.printf("PENCIL_RAW left=%u%s right=%u%s removed=%s\n",
      leftDistanceMm, leftReadingValid ? "" : " INVALID",
      rightDistanceMm, rightReadingValid ? "" : " INVALID",
      pencilRemoved ? "true" : "false");
  }
}

int findShell(const String &uid) {
  for (size_t index = 0; index < shellCount; ++index) if (shells[index].uid == uid) return static_cast<int>(index);
  return -1;
}

void saveShells() {
  JsonDocument document;
  JsonArray array = document.to<JsonArray>();
  for (size_t index = 0; index < shellCount; ++index) {
    JsonObject item = array.add<JsonObject>();
    item["uid"] = shells[index].uid;
    item["duration"] = shells[index].durationSeconds;
  }
  String json;
  serializeJson(document, json);
  preferences.putString("shells", json);
}

void loadShells() {
  preferences.begin("focus-lock", false);
  const String json = preferences.getString("shells", "[]");
  JsonDocument document;
  if (deserializeJson(document, json)) return;
  for (JsonObject item : document.as<JsonArray>()) {
    if (shellCount >= HardwareConfig::MAX_SHELLS) break;
    shells[shellCount].uid = item["uid"].as<String>();
    shells[shellCount].durationSeconds = item["duration"] | HardwareConfig::DEFAULT_SHELL_SECONDS;
    ++shellCount;
  }
}

bool setShellDuration(String uid, uint32_t durationSeconds) {
  uid.toUpperCase();
  int index = findShell(uid);
  if (index < 0) {
    if (shellCount >= HardwareConfig::MAX_SHELLS) return false;
    index = static_cast<int>(shellCount++);
    shells[index].uid = uid;
  }
  shells[index].durationSeconds = durationSeconds;
  saveShells();
  return true;
}

uint32_t durationForShell(const String &uid, bool &configured) {
  const int index = findShell(uid);
  configured = index >= 0;
  if (configured) return shells[index].durationSeconds;
  if (uid == HardwareConfig::MANUAL_SHELL_1_UID) return HardwareConfig::MANUAL_SHELL_1_DEFAULT_SECONDS;
  if (uid == HardwareConfig::MANUAL_SHELL_2_UID) return HardwareConfig::MANUAL_SHELL_2_DEFAULT_SECONDS;
  return HardwareConfig::DEFAULT_SHELL_SECONDS;
}

String readUid() {
  String uid;
  for (byte index = 0; index < rfid.uid.size; ++index) {
    if (rfid.uid.uidByte[index] < 0x10) uid += '0';
    uid += String(rfid.uid.uidByte[index], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void selectShell(const String &uid) {
  if (sessionActive) return;
  bool configured = false;
  selectedShellUid = uid;
  selectedDurationSeconds = durationForShell(uid, configured);
  targetTimeMs = selectedDurationSeconds * 1000UL;
  accumulatedTimeMs = 0;
  sessionState = SessionState::ARMED;
  pencilSeenAfterShellScan = !pencilRemoved;
  Serial.printf("Shell %s selected: %lu seconds (%s).\n", uid.c_str(), selectedDurationSeconds,
    configured ? "saved value" : "default value");
}

void updateRfid() {
  if (!HardwareConfig::RFID_ENABLED || !rfidReady || sessionActive) return;
  const unsigned long now = millis();
  if (now - lastRfidPollAt < 250) return;
  lastRfidPollAt = now;

  byte atqa[2];
  byte atqaSize = sizeof(atqa);
  const MFRC522::StatusCode wakeStatus = rfid.PICC_WakeupA(atqa, &atqaSize);
  if (wakeStatus != MFRC522::STATUS_OK && wakeStatus != MFRC522::STATUS_COLLISION) {
    if (now - lastRfidDiagnosticAt >= 1000) {
      lastRfidDiagnosticAt = now;
      Serial.printf("RFID_SCAN: waiting (%s)\n", rfid.GetStatusCodeName(wakeStatus));
    }
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) return;

  const String uid = readUid();
  if (uid != selectedShellUid) selectShell(uid);
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void runRfidSpiDiagnostics() {
  Serial.print("RFID_SPI version_reads=");
  for (byte index = 0; index < 8; ++index) {
    if (index) Serial.print(',');
    Serial.printf("%02X", rfid.PCD_ReadRegister(MFRC522::VersionReg));
    delay(2);
  }
  Serial.println();

  rfid.PCD_SetAntennaGain(rfid.RxGain_min);
  const byte minimumGainReadback = rfid.PCD_GetAntennaGain();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  const byte maximumGainReadback = rfid.PCD_GetAntennaGain();
  const byte txControl = rfid.PCD_ReadRegister(MFRC522::TxControlReg);

  const bool registerRoundTripPassed =
    minimumGainReadback == rfid.RxGain_min &&
    maximumGainReadback == rfid.RxGain_max;
  const bool antennaDriversEnabled = (txControl & 0x03) == 0x03;

  Serial.printf(
    "RFID_SPI gain_write_00_read_%02X gain_write_70_read_%02X round_trip=%s tx_control=%02X antenna_bits=%s self_test=NOT_RUN\n",
    minimumGainReadback,
    maximumGainReadback,
    registerRoundTripPassed ? "PASS" : "FAIL",
    txControl,
    antennaDriversEnabled ? "ON" : "OFF");
}

void beginSession() {
  if (selectedShellUid.isEmpty() || targetTimeMs == 0 || sessionActive) return;
  accumulatedTimeMs = 0;
  studyingStartedAt = millis();
  sessionActive = true;
  sessionState = SessionState::LOCKED_STUDYING;
  lockDrawer();
  playStartSound();
  Serial.printf("Session started from shell %s for %lu seconds.\n", selectedShellUid.c_str(), selectedDurationSeconds);
}

void completeSession() {
  accumulatedTimeMs = targetTimeMs;
  sessionActive = false;
  sessionState = SessionState::COMPLETE;
  stopShake();
  unlockDrawer();
  playCompleteSound();
  Serial.println("Session complete. Drawer unlocked.");
}

void updateSession() {
  if (sessionState == SessionState::ARMED) {
    if (!pencilRemoved) pencilSeenAfterShellScan = true;
    if (pencilRemoved && pencilSeenAfterShellScan) beginSession();
    return;
  }
  if (!sessionActive) return;

  if (pencilRemoved && sessionState == SessionState::LOCKED_PAUSED) {
    studyingStartedAt = millis();
    sessionState = SessionState::LOCKED_STUDYING;
  } else if (!pencilRemoved && sessionState == SessionState::LOCKED_STUDYING) {
    accumulatedTimeMs += millis() - studyingStartedAt;
    sessionState = SessionState::LOCKED_PAUSED;
    stopShake();
  }
  if (activeTimeMs() >= targetTimeMs) completeSession();
}

void updateCountdownDisplay() {
  if (!HardwareConfig::TM1637_ENABLED || millis() - lastDisplayUpdateAt < 200) return;
  lastDisplayUpdateAt = millis();
  if (sessionState == SessionState::WAITING_FOR_SHELL) {
    countdownDisplay.showString("----");
    return;
  }
  const uint32_t seconds = remainingSeconds();
  uint16_t displayValue;
  if (seconds < 100UL * 60UL) {
    displayValue = static_cast<uint16_t>((seconds / 60UL) * 100UL + seconds % 60UL);
  } else {
    const uint32_t minutes = (seconds + 59UL) / 60UL;
    displayValue = static_cast<uint16_t>((minutes / 60UL) * 100UL + minutes % 60UL);
  }
  countdownDisplay.showNumberDec(displayValue, 0b01000000, true, 4, 0);
}

void showEyeCharacters(uint8_t topCharacter, uint8_t bottomCharacter) {
  eyeDisplay.setCursor(4, 0);
  eyeDisplay.writeChar(topCharacter);
  eyeDisplay.setCursor(4, 1);
  eyeDisplay.writeChar(bottomCharacter);
  eyeDisplay.setCursor(11, 0);
  eyeDisplay.writeChar(topCharacter);
  eyeDisplay.setCursor(11, 1);
  eyeDisplay.writeChar(bottomCharacter);
}

void setEyeExpression(EyeExpression expression) {
  if (!HardwareConfig::LCD1602_ENABLED || expression == currentEyeExpression) return;

  switch (expression) {
    case EyeExpression::CENTER:
      eyeDisplay.createChar(0, CENTER_TOP);
      eyeDisplay.createChar(1, CENTER_BOTTOM);
      showEyeCharacters(0, 1);
      break;
    case EyeExpression::LEFT:
      showEyeCharacters(2, 3);
      break;
    case EyeExpression::RIGHT:
      showEyeCharacters(4, 5);
      break;
    case EyeExpression::BLINK:
      showEyeCharacters(6, 7);
      break;
    case EyeExpression::HAPPY:
      eyeDisplay.createChar(0, HAPPY_TOP);
      eyeDisplay.createChar(1, HAPPY_BOTTOM);
      showEyeCharacters(0, 1);
      break;
    case EyeExpression::SURPRISED:
      eyeDisplay.createChar(0, SURPRISED_TOP);
      eyeDisplay.createChar(1, SURPRISED_BOTTOM);
      showEyeCharacters(0, 1);
      break;
    case EyeExpression::SLEEP:
      eyeDisplay.createChar(0, SLEEP_TOP);
      eyeDisplay.createChar(1, SLEEP_BOTTOM);
      showEyeCharacters(0, 1);
      break;
  }
  currentEyeExpression = expression;
}

void updateEyeDisplay() {
  if (!HardwareConfig::LCD1602_ENABLED) return;
  const unsigned long now = millis();
  if (sessionState != lastEyeSessionState) {
    lastEyeSessionState = sessionState;
    eyeAnimationStartedAt = now;
    // Force a redraw because dynamic emotions reuse custom-character slots 0 and 1.
    currentEyeExpression = static_cast<EyeExpression>(255);
  }

  const unsigned long phase = now - eyeAnimationStartedAt;
  switch (sessionState) {
    case SessionState::ARMED:
      setEyeExpression(EyeExpression::SURPRISED);
      return;
    case SessionState::LOCKED_PAUSED:
      setEyeExpression(EyeExpression::SLEEP);
      return;
    case SessionState::COMPLETE:
      setEyeExpression(EyeExpression::HAPPY);
      return;
    case SessionState::WAITING_FOR_SHELL:
    case SessionState::LOCKED_STUDYING:
      break;
  }

  // A calm, non-blocking idle loop: center, glance left/right, and blink.
  const unsigned long cycle = phase % 7000UL;
  if (cycle < 2400UL) setEyeExpression(EyeExpression::CENTER);
  else if (cycle < 3200UL) setEyeExpression(EyeExpression::LEFT);
  else if (cycle < 4400UL) setEyeExpression(EyeExpression::CENTER);
  else if (cycle < 5200UL) setEyeExpression(EyeExpression::RIGHT);
  else if (cycle < 6400UL) setEyeExpression(EyeExpression::CENTER);
  else if (cycle < 6550UL) setEyeExpression(EyeExpression::BLINK);
  else setEyeExpression(EyeExpression::CENTER);
}

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
}

void logHttpRequest() {
  Serial.printf("HTTP %s from %s\n",
    server.uri().c_str(),
    server.client().remoteIP().toString().c_str());
}

void updateWifiDiagnostics() {
  const int clients = static_cast<int>(WiFi.softAPgetStationNum());
  if (clients == lastReportedApClients) return;
  lastReportedApClients = clients;
  Serial.printf("WIFI_AP clients=%d ip=%s\n", clients, WiFi.softAPIP().toString().c_str());
}

void loadServoSettings() {
  servoPreferences.begin("focus-servo", false);
  savedUnlockAngle = constrain(
    servoPreferences.getInt("unlock", HardwareConfig::SERVO_UNLOCK_ANGLE),
    HardwareConfig::SERVO_WEB_MIN_ANGLE,
    HardwareConfig::SERVO_WEB_MAX_ANGLE);
  savedLockAngle = constrain(
    servoPreferences.getInt("lock", HardwareConfig::SERVO_LOCK_ANGLE),
    HardwareConfig::SERVO_WEB_MIN_ANGLE,
    HardwareConfig::SERVO_WEB_MAX_ANGLE);
}

bool servoToolsAvailable() {
  return HardwareConfig::DEVELOPER_TOOLS_ENABLED &&
    (sessionState == SessionState::WAITING_FOR_SHELL || sessionState == SessionState::COMPLETE);
}

String servoStatusJson() {
  JsonDocument document;
  document["developerToolsEnabled"] = HardwareConfig::DEVELOPER_TOOLS_ENABLED;
  document["available"] = servoToolsAvailable();
  document["attached"] = lockServoAttached;
  document["sequenceActive"] = servoSequence.active;
  document["currentAngle"] = currentLockServoAngle;
  document["lockAngle"] = savedLockAngle;
  document["unlockAngle"] = savedUnlockAngle;
  document["completedRepetitions"] = servoSequence.completed;
  document["totalRepetitions"] = servoSequence.repetitions;
  document["minAngle"] = HardwareConfig::SERVO_WEB_MIN_ANGLE;
  document["maxAngle"] = HardwareConfig::SERVO_WEB_MAX_ANGLE;
  String payload;
  serializeJson(document, payload);
  return payload;
}

void sendServoStatus(int statusCode = 200) {
  addCorsHeaders();
  server.send(statusCode, "application/json", servoStatusJson());
}

bool requireServoTools() {
  if (!HardwareConfig::DEVELOPER_TOOLS_ENABLED) {
    addCorsHeaders();
    server.send(403, "text/plain", "Developer servo tools are disabled");
    return false;
  }
  if (!servoToolsAvailable()) {
    addCorsHeaders();
    server.send(409, "text/plain", "Reset or complete the study session before moving the servo");
    return false;
  }
  return true;
}

void handleServoMove() {
  if (!requireServoTools()) return;
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, server.arg("plain"));
  const int angle = document["angle"] | -1;
  if (error || angle < HardwareConfig::SERVO_WEB_MIN_ANGLE || angle > HardwareConfig::SERVO_WEB_MAX_ANGLE) {
    addCorsHeaders();
    server.send(400, "text/plain", "Angle must be from 0 to 180 degrees");
    return;
  }
  servoSequence.active = false;
  if (!attachLockServo()) {
    addCorsHeaders();
    server.send(500, "text/plain", "Could not attach the lock servo");
    return;
  }
  writeLockServo(angle);
  Serial.printf("SERVO_WEB move=%d\n", angle);
  sendServoStatus();
}

void handleServoSequenceStart() {
  if (!requireServoTools()) return;
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, server.arg("plain"));
  const int outerAngle = document["outerAngle"] | -1;
  const int innerAngle = document["innerAngle"] | -1;
  const int repetitions = document["repetitions"] | 0;
  const int holdMs = document["holdMs"] | 0;
  if (error ||
      outerAngle < HardwareConfig::SERVO_WEB_MIN_ANGLE || outerAngle > HardwareConfig::SERVO_WEB_MAX_ANGLE ||
      innerAngle < HardwareConfig::SERVO_WEB_MIN_ANGLE || innerAngle > HardwareConfig::SERVO_WEB_MAX_ANGLE ||
      repetitions < 1 || repetitions > static_cast<int>(HardwareConfig::SERVO_WEB_MAX_REPETITIONS) ||
      holdMs < static_cast<int>(HardwareConfig::SERVO_WEB_MIN_HOLD_MS) ||
      holdMs > static_cast<int>(HardwareConfig::SERVO_WEB_MAX_HOLD_MS)) {
    addCorsHeaders();
    server.send(400, "text/plain", "Invalid sequence settings");
    return;
  }
  if (!attachLockServo()) {
    addCorsHeaders();
    server.send(500, "text/plain", "Could not attach the lock servo");
    return;
  }
  servoSequence.outerAngle = outerAngle;
  servoSequence.innerAngle = innerAngle;
  servoSequence.repetitions = static_cast<unsigned int>(repetitions);
  servoSequence.completed = 0;
  servoSequence.holdMs = static_cast<unsigned int>(holdMs);
  servoSequence.leg = 0;
  servoSequence.active = true;
  writeLockServo(outerAngle);
  servoSequence.changedAt = millis();
  Serial.printf("SERVO_WEB sequence=%d->%d->%d repeats=%d hold=%d\n",
    outerAngle, innerAngle, outerAngle, repetitions, holdMs);
  sendServoStatus(202);
}

void handleServoDetach() {
  if (!HardwareConfig::DEVELOPER_TOOLS_ENABLED) {
    addCorsHeaders();
    server.send(403, "text/plain", "Developer servo tools are disabled");
    return;
  }
  detachLockServo();
  Serial.println("SERVO_WEB detached");
  sendServoStatus();
}

void handleServoSave() {
  if (!requireServoTools()) return;
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, server.arg("plain"));
  String position = document["position"] | "";
  const int angle = document["angle"] | -1;
  position.toLowerCase();
  if (error || (position != "lock" && position != "unlock") ||
      angle < HardwareConfig::SERVO_WEB_MIN_ANGLE || angle > HardwareConfig::SERVO_WEB_MAX_ANGLE) {
    addCorsHeaders();
    server.send(400, "text/plain", "Position must be lock or unlock and angle must be 0 to 180");
    return;
  }
  if (position == "lock") {
    savedLockAngle = angle;
    servoPreferences.putInt("lock", angle);
  } else {
    savedUnlockAngle = angle;
    servoPreferences.putInt("unlock", angle);
  }
  Serial.printf("SERVO_WEB saved %s=%d\n", position.c_str(), angle);
  sendServoStatus();
}

void updateServoSequence() {
  if (!servoSequence.active) return;
  if (!lockServoAttached) {
    servoSequence.active = false;
    return;
  }
  if (millis() - servoSequence.changedAt < servoSequence.holdMs) return;

  if (servoSequence.leg == 0) {
    writeLockServo(servoSequence.innerAngle);
    servoSequence.leg = 1;
  } else if (servoSequence.leg == 1) {
    writeLockServo(servoSequence.outerAngle);
    ++servoSequence.completed;
    servoSequence.leg = 2;
  } else if (servoSequence.completed >= servoSequence.repetitions) {
    detachLockServo();
    Serial.println("SERVO_WEB sequence complete; signal detached");
    return;
  } else {
    writeLockServo(servoSequence.innerAngle);
    servoSequence.leg = 1;
  }
  servoSequence.changedAt = millis();
}

String statusJson() {
  JsonDocument document;
  const uint32_t active = activeTimeMs();
  bool configured = false;
  if (!selectedShellUid.isEmpty()) durationForShell(selectedShellUid, configured);
  document["state"] = stateName();
  document["sessionActive"] = sessionActive;
  document["locked"] = drawerLocked;
  document["pencilPresent"] = !pencilRemoved;
  document["pencilRemoved"] = pencilRemoved;
  document["activeTime"] = active / 1000UL;
  document["targetTime"] = targetTimeMs / 1000UL;
  document["remainingTime"] = remainingSeconds();
  document["sensorsReady"] = sensorsReady;
  document["rfidReady"] = rfidReady;
  document["servoEnabled"] = HardwareConfig::SERVO_ENABLED;
  document["developerToolsEnabled"] = HardwareConfig::DEVELOPER_TOOLS_ENABLED;
  document["activeTagUid"] = selectedShellUid;
  document["selectedDuration"] = selectedDurationSeconds;
  document["shellConfigured"] = configured;
  document["leftPencilMm"] = leftDistanceMm;
  document["rightPencilMm"] = rightDistanceMm;
  document["leftReadingValid"] = leftReadingValid;
  document["rightReadingValid"] = rightReadingValid;
  String payload;
  serializeJson(document, payload);
  return payload;
}

void sendStatus(int statusCode = 200) {
  addCorsHeaders();
  server.send(statusCode, "application/json", statusJson());
}

void handleShellSave() {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, server.arg("plain"));
  String uid = document["uid"] | "";
  const uint32_t durationSeconds = document["duration"] | 0UL;
  uid.trim(); uid.toUpperCase();
  if (error || uid.isEmpty() || durationSeconds == 0 || durationSeconds > HardwareConfig::MAX_SESSION_SECONDS) {
    addCorsHeaders();
    server.send(400, "text/plain", "UID and a duration from 1 second to 8 hours are required");
    return;
  }
  if (!setShellDuration(uid, durationSeconds)) {
    addCorsHeaders();
    server.send(409, "text/plain", "Shell storage is full");
    return;
  }
  if (selectedShellUid == uid && !sessionActive) {
    selectedDurationSeconds = durationSeconds;
    targetTimeMs = durationSeconds * 1000UL;
  }
  sendStatus();
}

bool isManualShell(const String &uid) {
  return uid == HardwareConfig::MANUAL_SHELL_1_UID || uid == HardwareConfig::MANUAL_SHELL_2_UID;
}

void handleShellSelect() {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, server.arg("plain"));
  String uid = document["uid"] | "";
  uid.trim();
  uid.toUpperCase();
  if (error || !isManualShell(uid) || sessionActive) {
    addCorsHeaders();
    server.send(400, "text/plain", "Select SHELL1 or SHELL2 while no session is active");
    return;
  }
  selectShell(uid);
  sendStatus();
}

void handleShellList() {
  JsonDocument document;
  JsonArray array = document["shells"].to<JsonArray>();
  for (size_t index = 0; index < shellCount; ++index) {
    JsonObject item = array.add<JsonObject>();
    item["uid"] = shells[index].uid;
    item["duration"] = shells[index].durationSeconds;
  }
  String payload;
  serializeJson(document, payload);
  addCorsHeaders();
  server.send(200, "application/json", payload);
}

void handleDeveloperStart() {
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, server.arg("plain"));
  const uint32_t durationSeconds = document["duration"] | 0UL;
  if (error || durationSeconds == 0 || durationSeconds > HardwareConfig::MAX_SESSION_SECONDS) {
    addCorsHeaders();
    server.send(400, "text/plain", "Invalid duration");
    return;
  }
  selectedShellUid = "DEMO";
  selectedDurationSeconds = durationSeconds;
  targetTimeMs = durationSeconds * 1000UL;
  sessionState = SessionState::ARMED;
  pencilSeenAfterShellScan = !pencilRemoved;
  sendStatus();
}

void handleReset() {
  sessionActive = false;
  targetTimeMs = 0;
  accumulatedTimeMs = 0;
  selectedShellUid = "";
  sessionState = SessionState::WAITING_FOR_SHELL;
  stopShake();
  unlockDrawer();
  sendStatus();
}

String contentTypeFor(const String &path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "text/javascript";
  if (path.endsWith(".json")) return "application/json";
  return "text/plain";
}

bool serveFile(String path) {
  if (!filesystemReady) return false;
  if (path == "/") path = "/index.html";
  if (!LittleFS.exists(path)) return false;
  File file = LittleFS.open(path, "r");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.streamFile(file, contentTypeFor(path));
  file.close();
  return true;
}

void configureWebServer() {
  server.on("/status", HTTP_GET, []() { logHttpRequest(); sendStatus(); });
  server.on("/shells", HTTP_GET, handleShellList);
  server.on("/shell", HTTP_POST, handleShellSave);
  server.on("/shell/select", HTTP_POST, handleShellSelect);
  server.on("/start", HTTP_POST, handleDeveloperStart);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/servo/status", HTTP_GET, []() { logHttpRequest(); sendServoStatus(); });
  server.on("/servo/move", HTTP_POST, handleServoMove);
  server.on("/servo/sequence", HTTP_POST, handleServoSequenceStart);
  server.on("/servo/detach", HTTP_POST, handleServoDetach);
  server.on("/servo/save", HTTP_POST, handleServoSave);
  server.on("/start", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.on("/shell", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.on("/shell/select", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.on("/servo/move", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.on("/servo/sequence", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.on("/servo/detach", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.on("/servo/save", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.onNotFound([]() {
    logHttpRequest();
    if (!serveFile(server.uri())) server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

void handleServoCalibration() {
  if (!HardwareConfig::SERVO_CALIBRATION_MODE || !lockServoAttached || !Serial.available()) return;
  const char command = Serial.read();
  static int angle = HardwareConfig::SERVO_CALIBRATION_START_ANGLE;
  if (command == '+') angle = min(180, angle + 5);
  else if (command == '-') angle = max(0, angle - 5);
  else if (command == 'u') angle = HardwareConfig::SERVO_UNLOCK_ANGLE;
  else if (command == 'l') angle = HardwareConfig::SERVO_LOCK_ANGLE;
  else return;
  writeLockServo(angle);
  Serial.printf("LOCK_SERVO_ANGLE=%d\n", angle);
}

void runServoCycleTest() {
  if (!HardwareConfig::SERVO_CYCLE_TEST_MODE || !lockServoAttached) return;

  const int originalAngle = HardwareConfig::SERVO_CYCLE_ORIGINAL_ANGLE;
  const int ccwAngle = HardwareConfig::SERVO_CYCLE_CCW_ANGLE;
  writeLockServo(originalAngle);
  delay(HardwareConfig::SERVO_CYCLE_HOLD_MS);

  for (unsigned int cycle = 1; cycle <= HardwareConfig::SERVO_CYCLE_REPETITIONS; ++cycle) {
    Serial.printf("SERVO_CYCLE %u/%u: %d -> %d -> %d\n",
      cycle, HardwareConfig::SERVO_CYCLE_REPETITIONS, originalAngle, ccwAngle, originalAngle);

    for (int angle = originalAngle; angle >= ccwAngle; --angle) {
      writeLockServo(angle);
      delay(HardwareConfig::SERVO_CYCLE_STEP_DELAY_MS);
    }
    delay(HardwareConfig::SERVO_CYCLE_HOLD_MS);

    for (int angle = ccwAngle; angle <= originalAngle; ++angle) {
      writeLockServo(angle);
      delay(HardwareConfig::SERVO_CYCLE_STEP_DELAY_MS);
    }
    delay(HardwareConfig::SERVO_CYCLE_HOLD_MS);
  }

  writeLockServo(originalAngle);
  delay(HardwareConfig::SERVO_CYCLE_HOLD_MS);
  lockServo.detach();
  lockServoAttached = false;
  Serial.println("SERVO_CYCLE complete; servo returned to 90 degrees and detached.");
}

void runServoSwiftTest() {
  if (!HardwareConfig::SERVO_SWIFT_TEST_MODE || !lockServoAttached) return;

  Serial.println("SERVO_SWIFT physical 270 -> 180 -> 270 (commands 90 -> 0 -> 90)");
  writeLockServo(HardwareConfig::SERVO_SWIFT_270_COMMAND);
  delay(HardwareConfig::SERVO_SWIFT_HOLD_MS);
  writeLockServo(HardwareConfig::SERVO_SWIFT_180_COMMAND);
  delay(HardwareConfig::SERVO_SWIFT_HOLD_MS);
  writeLockServo(HardwareConfig::SERVO_SWIFT_270_COMMAND);
  delay(HardwareConfig::SERVO_SWIFT_HOLD_MS);

  lockServo.detach();
  lockServoAttached = false;
  Serial.println("SERVO_SWIFT complete at physical 270 (servo command 90); signal detached.");
}

void setupDisplays() {
  if (HardwareConfig::TM1637_ENABLED) {
    countdownDisplay.setBrightness(HardwareConfig::TM1637_BRIGHTNESS);
    countdownDisplay.showString("----");
  }
  if (HardwareConfig::LCD1602_ENABLED) {
    eyeDisplay.begin();
    eyeDisplay.createChar(2, LEFT_TOP);
    eyeDisplay.createChar(3, LEFT_BOTTOM);
    eyeDisplay.createChar(4, RIGHT_TOP);
    eyeDisplay.createChar(5, RIGHT_BOTTOM);
    eyeDisplay.createChar(6, BLINK_TOP);
    eyeDisplay.createChar(7, BLINK_BOTTOM);
    currentEyeExpression = static_cast<EyeExpression>(255);
    setEyeExpression(EyeExpression::CENTER);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nFocus Lock booting...");
  loadServoSettings();

  if (HardwareConfig::SERVO_ENABLED || HardwareConfig::SERVO_CALIBRATION_MODE ||
      HardwareConfig::SERVO_CYCLE_TEST_MODE || HardwareConfig::SERVO_SWIFT_TEST_MODE) {
    lockServo.setPeriodHertz(50);
    lockServo.attach(Pins::LOCK_SERVO, HardwareConfig::SERVO_MIN_PULSE_US, HardwareConfig::SERVO_MAX_PULSE_US);
    lockServoAttached = lockServo.attached();
    if (HardwareConfig::SERVO_SWIFT_TEST_MODE) runServoSwiftTest();
    else if (HardwareConfig::SERVO_CYCLE_TEST_MODE) runServoCycleTest();
    else if (HardwareConfig::SERVO_CALIBRATION_MODE) writeLockServo(HardwareConfig::SERVO_CALIBRATION_START_ANGLE);
    else unlockDrawer();
  } else {
    Serial.println("SAFE MODE: Lock servo output disabled until calibration is complete.");
  }

  if (HardwareConfig::SHAKE_SERVO_ENABLED) {
    shakeServo.setPeriodHertz(50);
    shakeServo.attach(Pins::SHAKE_SERVO, HardwareConfig::SERVO_MIN_PULSE_US, HardwareConfig::SERVO_MAX_PULSE_US);
    shakeServoAttached = shakeServo.attached();
    stopShake();
  }

  if (HardwareConfig::BUZZER_ENABLED || HardwareConfig::BUZZER_TEST_MODE) {
    ledcSetup(HardwareConfig::BUZZER_PWM_CHANNEL, 2000, 8);
    ledcAttachPin(Pins::BUZZER, HardwareConfig::BUZZER_PWM_CHANNEL);
    if (HardwareConfig::BUZZER_TEST_MODE) playTone(880, 250);
  }

  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  sensorsReady = initializeSensors();
  setupDisplays();

  if (HardwareConfig::RFID_ENABLED) {
    SPI.begin(Pins::RFID_SCK, Pins::RFID_MISO, Pins::RFID_MOSI, Pins::RFID_CS);
    rfid.PCD_Init();
    runRfidSpiDiagnostics();
    const byte rfidVersion = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    rfidReady = rfidVersion != 0x00 && rfidVersion != 0xFF;
    Serial.printf("RC522: %s (version 0x%02X)\n", rfidReady ? "ready" : "not found", rfidVersion);
  } else {
    rfidReady = false;
    Serial.println("RFID bypassed: select Shell 1 or Shell 2 from the website.");
  }

  loadShells();
  filesystemReady = LittleFS.begin(true);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  const bool accessPointStarted = WiFi.softAP(HardwareConfig::ACCESS_POINT_NAME, nullptr, 1, false, 4);
  configureWebServer();
  Serial.printf("Wi-Fi AP start: %s; SSID='%s'; channel=1; IP=http://%s\n",
    accessPointStarted ? "SUCCESS" : "FAILED",
    HardwareConfig::ACCESS_POINT_NAME,
    WiFi.softAPIP().toString().c_str());
}

void loop() {
  server.handleClient();
  updateWifiDiagnostics();
  updateServoSequence();
  updatePencilSensors();
  updateRfid();
  updateSession();
  updateCountdownDisplay();
  updateEyeDisplay();
  updateShake();
  handleServoCalibration();
  delay(2);
}
