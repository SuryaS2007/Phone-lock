#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <LittleFS.h>
#include <VL53L0X.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

#include "HardwareConfig.h"
#include "PinConfig.h"

enum class SessionState {
  READY,
  LOCKED_PAUSED,
  LOCKED_STUDYING,
  COMPLETE
};

WebServer server(80);
VL53L0X leftClawSensor;
VL53L0X rightClawSensor;
Servo lockServo;

bool sensorsReady = false;
bool filesystemReady = false;
bool servoAttached = false;
bool drawerLocked = false;

uint16_t leftDistanceMm = 0;
uint16_t rightDistanceMm = 0;
bool leftReadingValid = false;
bool rightReadingValid = false;
bool clawsLifted = false;
bool pendingClawsLifted = false;
unsigned long pendingClawChangeAt = 0;
unsigned long lastSensorReadAt = 0;
unsigned long lastDiagnosticAt = 0;

SessionState sessionState = SessionState::READY;
bool sessionActive = false;
uint32_t targetTimeMs = 0;
uint32_t accumulatedTimeMs = 0;
uint32_t studyingStartedAt = 0;

const char *stateName() {
  switch (sessionState) {
    case SessionState::READY: return "READY";
    case SessionState::LOCKED_PAUSED: return "LOCKED_PAUSED";
    case SessionState::LOCKED_STUDYING: return "LOCKED_STUDYING";
    case SessionState::COMPLETE: return "COMPLETE";
  }
  return "READY";
}

uint32_t activeTimeMs() {
  uint32_t active = accumulatedTimeMs;
  if (sessionActive && sessionState == SessionState::LOCKED_STUDYING) {
    active += millis() - studyingStartedAt;
  }
  return targetTimeMs > 0 ? min(active, targetTimeMs) : active;
}

void writeServoAngle(int angle) {
  if (!servoAttached) return;
  lockServo.write(constrain(angle, 0, 180));
  delay(250);
}

void unlockDrawer() {
  if (HardwareConfig::SERVO_ENABLED || HardwareConfig::SERVO_CALIBRATION_MODE) {
    writeServoAngle(HardwareConfig::SERVO_UNLOCK_ANGLE);
  }
  drawerLocked = false;
}

void lockDrawer() {
  if (HardwareConfig::SERVO_ENABLED) {
    writeServoAngle(HardwareConfig::SERVO_LOCK_ANGLE);
    drawerLocked = true;
  } else {
    drawerLocked = false;
    Serial.println("[SAFE MODE] Lock requested, but SERVO_ENABLED is false.");
  }
}

bool initializeSensors() {
  pinMode(Pins::LEFT_CLAW_XSHUT, OUTPUT);
  pinMode(Pins::RIGHT_CLAW_XSHUT, OUTPUT);
  digitalWrite(Pins::LEFT_CLAW_XSHUT, LOW);
  digitalWrite(Pins::RIGHT_CLAW_XSHUT, LOW);
  delay(20);

  digitalWrite(Pins::LEFT_CLAW_XSHUT, HIGH);
  delay(20);
  if (!leftClawSensor.init()) {
    Serial.println("ERROR: Left claw VL53L0X was not found.");
    return false;
  }
  leftClawSensor.setAddress(I2cAddresses::LEFT_CLAW);
  leftClawSensor.setTimeout(HardwareConfig::SENSOR_TIMEOUT_MS);

  digitalWrite(Pins::RIGHT_CLAW_XSHUT, HIGH);
  delay(20);
  if (!rightClawSensor.init()) {
    Serial.println("ERROR: Right claw VL53L0X was not found.");
    return false;
  }
  rightClawSensor.setAddress(I2cAddresses::RIGHT_CLAW);
  rightClawSensor.setTimeout(HardwareConfig::SENSOR_TIMEOUT_MS);

  leftClawSensor.startContinuous(HardwareConfig::SENSOR_PERIOD_MS);
  rightClawSensor.startContinuous(HardwareConfig::SENSOR_PERIOD_MS);
  Serial.println("Both claw sensors initialized at 0x30 and 0x31.");
  return true;
}

bool readingMeansLifted(uint16_t distance, uint16_t threshold) {
  return HardwareConfig::CLAW_LIFTED_WHEN_DISTANCE_GREATER
    ? distance > threshold
    : distance < threshold;
}

void updateClawSensors() {
  const unsigned long now = millis();
  if (!sensorsReady || now - lastSensorReadAt < HardwareConfig::SENSOR_PERIOD_MS) return;
  lastSensorReadAt = now;

  leftDistanceMm = leftClawSensor.readRangeContinuousMillimeters();
  leftReadingValid = !leftClawSensor.timeoutOccurred() && leftDistanceMm < 8190;
  rightDistanceMm = rightClawSensor.readRangeContinuousMillimeters();
  rightReadingValid = !rightClawSensor.timeoutOccurred() && rightDistanceMm < 8190;

  if (leftReadingValid && rightReadingValid) {
    const bool leftLifted = readingMeansLifted(leftDistanceMm, HardwareConfig::LEFT_CLAW_THRESHOLD_MM);
    const bool rightLifted = readingMeansLifted(rightDistanceMm, HardwareConfig::RIGHT_CLAW_THRESHOLD_MM);
    const bool rawClawsLifted = leftLifted && rightLifted;

    if (rawClawsLifted != pendingClawsLifted) {
      pendingClawsLifted = rawClawsLifted;
      pendingClawChangeAt = now;
    } else if (clawsLifted != pendingClawsLifted && now - pendingClawChangeAt >= HardwareConfig::CLAW_DEBOUNCE_MS) {
      clawsLifted = pendingClawsLifted;
      Serial.printf("Claws: %s\n", clawsLifted ? "LIFTED / STUDYING" : "DOWN / PAUSED");
    }
  }

  if (HardwareConfig::SENSOR_DIAGNOSTICS && now - lastDiagnosticAt >= 500) {
    lastDiagnosticAt = now;
    Serial.printf("CLAW_RAW left=%u%s right=%u%s lifted=%s\n",
      leftDistanceMm, leftReadingValid ? "" : " INVALID",
      rightDistanceMm, rightReadingValid ? "" : " INVALID",
      clawsLifted ? "true" : "false");
  }
}

void completeSession() {
  accumulatedTimeMs = targetTimeMs;
  sessionActive = false;
  sessionState = SessionState::COMPLETE;
  unlockDrawer();
  Serial.println("Session complete. Drawer unlocked.");
}

void updateSession() {
  if (!sessionActive) return;

  if (clawsLifted && sessionState == SessionState::LOCKED_PAUSED) {
    studyingStartedAt = millis();
    sessionState = SessionState::LOCKED_STUDYING;
  } else if (!clawsLifted && sessionState == SessionState::LOCKED_STUDYING) {
    accumulatedTimeMs += millis() - studyingStartedAt;
    sessionState = SessionState::LOCKED_PAUSED;
  }

  if (activeTimeMs() >= targetTimeMs) completeSession();
}

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
}

String statusJson() {
  JsonDocument document;
  const uint32_t active = activeTimeMs();
  document["state"] = stateName();
  document["sessionActive"] = sessionActive;
  document["locked"] = drawerLocked;
  document["pencilPresent"] = !clawsLifted;
  document["clawsLifted"] = clawsLifted;
  document["activeTime"] = active / 1000;
  document["targetTime"] = targetTimeMs / 1000;
  document["remainingTime"] = targetTimeMs > active ? (targetTimeMs - active) / 1000 : 0;
  document["sensorsReady"] = sensorsReady;
  document["servoEnabled"] = HardwareConfig::SERVO_ENABLED;
  document["leftClawMm"] = leftDistanceMm;
  document["rightClawMm"] = rightDistanceMm;
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

void handleStart() {
  if (!sensorsReady) {
    addCorsHeaders();
    server.send(503, "text/plain", "Claw sensors are not ready");
    return;
  }

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, server.arg("plain"));
  const unsigned long durationSeconds = document["duration"] | 0UL;
  if (error || durationSeconds == 0 || durationSeconds > HardwareConfig::MAX_SESSION_SECONDS) {
    addCorsHeaders();
    server.send(400, "text/plain", "Duration must be between 1 second and 8 hours");
    return;
  }

  targetTimeMs = durationSeconds * 1000UL;
  accumulatedTimeMs = 0;
  sessionActive = true;
  lockDrawer();

  if (clawsLifted) {
    studyingStartedAt = millis();
    sessionState = SessionState::LOCKED_STUDYING;
  } else {
    sessionState = SessionState::LOCKED_PAUSED;
  }

  Serial.printf("Session started: %lu seconds, initial state %s.\n", durationSeconds, stateName());
  sendStatus();
}

void handleReset() {
  sessionActive = false;
  targetTimeMs = 0;
  accumulatedTimeMs = 0;
  sessionState = SessionState::READY;
  unlockDrawer();
  sendStatus();
}

String contentTypeFor(const String &path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "text/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".svg")) return "image/svg+xml";
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
  server.on("/start", HTTP_POST, handleStart);
  server.on("/start", HTTP_OPTIONS, []() { addCorsHeaders(); server.send(204); });
  server.on("/reset", HTTP_POST, handleReset);
  server.onNotFound([]() {
    if (!serveFile(server.uri())) server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

void handleServoCalibration() {
  if (!HardwareConfig::SERVO_CALIBRATION_MODE || !servoAttached || !Serial.available()) return;
  const char command = Serial.read();
  static int angle = HardwareConfig::SERVO_CALIBRATION_START_ANGLE;
  if (command == '+') angle = min(180, angle + 5);
  else if (command == '-') angle = max(0, angle - 5);
  else if (command == 'u') angle = HardwareConfig::SERVO_UNLOCK_ANGLE;
  else if (command == 'l') angle = HardwareConfig::SERVO_LOCK_ANGLE;
  else return;
  writeServoAngle(angle);
  Serial.printf("SERVO_ANGLE=%d\n", angle);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nFocus Lock booting...");

  if (HardwareConfig::SERVO_ENABLED || HardwareConfig::SERVO_CALIBRATION_MODE) {
    lockServo.setPeriodHertz(50);
    lockServo.attach(Pins::LOCK_SERVO, HardwareConfig::SERVO_MIN_PULSE_US, HardwareConfig::SERVO_MAX_PULSE_US);
    servoAttached = lockServo.attached();
    if (HardwareConfig::SERVO_CALIBRATION_MODE) {
      writeServoAngle(HardwareConfig::SERVO_CALIBRATION_START_ANGLE);
    } else {
      unlockDrawer();
    }
    Serial.printf("Servo attached: %s\n", servoAttached ? "yes" : "no");
  } else {
    Serial.println("SAFE MODE: Servo output disabled until calibration is complete.");
  }

  Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);
  sensorsReady = initializeSensors();
  filesystemReady = LittleFS.begin(true);
  Serial.printf("LittleFS: %s\n", filesystemReady ? "ready" : "not available");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(HardwareConfig::ACCESS_POINT_NAME);
  Serial.printf("Connect to Wi-Fi '%s' and open http://%s\n",
    HardwareConfig::ACCESS_POINT_NAME, WiFi.softAPIP().toString().c_str());

  configureWebServer();
}

void loop() {
  server.handleClient();
  updateClawSensors();
  updateSession();
  handleServoCalibration();
  delay(2);
}
