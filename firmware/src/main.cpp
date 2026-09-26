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

WebServer server(80);
VL53L0X leftPencilSensor;
VL53L0X rightPencilSensor;
MFRC522 rfid(Pins::RFID_CS, Pins::RFID_RST);
TM1637TinyDisplay countdownDisplay(Pins::DISPLAY_CLK, Pins::DISPLAY_DIO);
Lcd1602I2c eyeDisplay(HardwareConfig::LCD1602_ADDRESS);
Servo lockServo;
Servo shakeServo;
Preferences preferences;

ShellSetting shells[HardwareConfig::MAX_SHELLS];
size_t shellCount = 0;
String selectedShellUid;
uint32_t selectedDurationSeconds = HardwareConfig::DEFAULT_SHELL_SECONDS;

bool sensorsReady = false;
bool rfidReady = false;
bool filesystemReady = false;
bool lockServoAttached = false;
bool shakeServoAttached = false;
bool drawerLocked = false;

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
unsigned long lastDisplayUpdateAt = 0;
unsigned long lastShakeAt = 0;
bool shakeDirection = false;

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
  if (!HardwareConfig::BUZZER_ENABLED) return;
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
  lockServo.write(constrain(angle, 0, 180));
  delay(250);
}

void unlockDrawer() {
  if (HardwareConfig::SERVO_ENABLED || HardwareConfig::SERVO_CALIBRATION_MODE) {
    writeLockServo(HardwareConfig::SERVO_UNLOCK_ANGLE);
  }
  drawerLocked = false;
}

void lockDrawer() {
  if (HardwareConfig::SERVO_ENABLED) {
    writeLockServo(HardwareConfig::SERVO_LOCK_ANGLE);
    drawerLocked = true;
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
  if (!leftPencilSensor.init()) {
    Serial.println("ERROR: Left pencil VL53L0X was not found.");
    return false;
  }
  leftPencilSensor.setAddress(I2cAddresses::LEFT_PENCIL);
  leftPencilSensor.setTimeout(HardwareConfig::SENSOR_TIMEOUT_MS);

  digitalWrite(Pins::RIGHT_PENCIL_XSHUT, HIGH);
  delay(20);
  if (!rightPencilSensor.init()) {
    Serial.println("ERROR: Right pencil VL53L0X was not found.");
    return false;
  }
  rightPencilSensor.setAddress(I2cAddresses::RIGHT_PENCIL);
  rightPencilSensor.setTimeout(HardwareConfig::SENSOR_TIMEOUT_MS);
  leftPencilSensor.startContinuous(HardwareConfig::SENSOR_PERIOD_MS);
  rightPencilSensor.startContinuous(HardwareConfig::SENSOR_PERIOD_MS);
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

  leftDistanceMm = leftPencilSensor.readRangeContinuousMillimeters();
  leftReadingValid = !leftPencilSensor.timeoutOccurred() && leftDistanceMm < 8190;
  rightDistanceMm = rightPencilSensor.readRangeContinuousMillimeters();
  rightReadingValid = !rightPencilSensor.timeoutOccurred() && rightDistanceMm < 8190;

  if (leftReadingValid && rightReadingValid) {
    const bool leftPresent = readingMeansPencilPresent(leftDistanceMm, HardwareConfig::LEFT_PENCIL_THRESHOLD_MM);
    const bool rightPresent = readingMeansPencilPresent(rightDistanceMm, HardwareConfig::RIGHT_PENCIL_THRESHOLD_MM);
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
  return configured ? shells[index].durationSeconds : HardwareConfig::DEFAULT_SHELL_SECONDS;
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
  if (!rfidReady || sessionActive) return;
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  selectShell(readUid());
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
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

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
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
  server.streamFile(file, contentTypeFor(path));
  file.close();
  return true;
}

void configureWebServer() {
  server.on("/status", HTTP_GET, []() { sendStatus(); });
  server.on("/shells", HTTP_GET, handleShellList);
  server.on("/shell", HTTP_POST, handleShellSave);
  server.on("/start", HTTP_POST, handleDeveloperStart);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/start", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.on("/shell", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.onNotFound([]() { if (!serveFile(server.uri())) server.send(404, "text/plain", "Not found"); });
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

void setupDisplays() {
  if (HardwareConfig::TM1637_ENABLED) {
    countdownDisplay.setBrightness(HardwareConfig::TM1637_BRIGHTNESS);
    countdownDisplay.showString("----");
  }
  if (HardwareConfig::LCD1602_ENABLED) {
    eyeDisplay.begin();
    eyeDisplay.printLine(0, "   (o)    (o)   ");
    eyeDisplay.printLine(1, "      \\__/      ");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nFocus Lock booting...");

  if (HardwareConfig::SERVO_ENABLED || HardwareConfig::SERVO_CALIBRATION_MODE) {
    lockServo.setPeriodHertz(50);
    lockServo.attach(Pins::LOCK_SERVO, HardwareConfig::SERVO_MIN_PULSE_US, HardwareConfig::SERVO_MAX_PULSE_US);
    lockServoAttached = lockServo.attached();
    if (HardwareConfig::SERVO_CALIBRATION_MODE) writeLockServo(HardwareConfig::SERVO_CALIBRATION_START_ANGLE);
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

  if (HardwareConfig::BUZZER_ENABLED) {
    ledcSetup(HardwareConfig::BUZZER_PWM_CHANNEL, 2000, 8);
    ledcAttachPin(Pins::BUZZER, HardwareConfig::BUZZER_PWM_CHANNEL);
  }

  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  sensorsReady = initializeSensors();
  setupDisplays();

  SPI.begin(Pins::RFID_SCK, Pins::RFID_MISO, Pins::RFID_MOSI, Pins::RFID_CS);
  rfid.PCD_Init();
  const byte rfidVersion = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  rfidReady = rfidVersion != 0x00 && rfidVersion != 0xFF;
  Serial.printf("RC522: %s (version 0x%02X)\n", rfidReady ? "ready" : "not found", rfidVersion);

  loadShells();
  filesystemReady = LittleFS.begin(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(HardwareConfig::ACCESS_POINT_NAME);
  configureWebServer();
  Serial.printf("Connect to Wi-Fi '%s' and open http://%s\n",
    HardwareConfig::ACCESS_POINT_NAME, WiFi.softAPIP().toString().c_str());
}

void loop() {
  server.handleClient();
  updatePencilSensors();
  updateRfid();
  updateSession();
  updateCountdownDisplay();
  updateShake();
  handleServoCalibration();
  delay(2);
}
