# Focus Lock

Focus Lock is an ESP32-powered phone lock and active-study timer. RFID tags embedded in physical shells identify saved study durations. The website changes the duration associated with each shell. After scanning a shell, lifting the pencil from its fixed crab-claw rest locks the phone drawer and starts the countdown; returning the pencil pauses it. Reaching zero unlocks the drawer.

## Final prototype flow

```text
Scan RFID shell
-> ESP32 loads that shell's saved duration
-> optionally change/save its duration on the website
-> place pencil on the fixed claws
-> lift pencil
-> drawer locks, buzzer sounds, countdown starts
-> return pencil to pause
-> lift it to resume
-> countdown reaches zero
-> drawer unlocks and buzzer sounds
```

RFID UIDs are hardware identifiers and are not rewritten. The website stores an editable UID-to-duration association in ESP32 nonvolatile memory.

## Hardware roles

- Generic 30-pin ESP32 DevKit V1: controller, Wi-Fi access point, API, timer, and website host.
- Two VL53L0X sensors: detect whether the pencil rests across the stationary claws.
- RC522: reads the RFID tag inside a shell and selects its duration.
- SG90 on GPIO26: locks and unlocks the phone drawer.
- SG90 on GPIO25: optionally sways the crab while active study time is counting.
- TM1637 on GPIO14/GPIO13: physical countdown only.
- LCD1602 at I2C address `0x27`: displays the crab-eye graphic.
- Passive buzzer on GPIO17: start and completion sounds.
- There is no phone-box sensor, reward system, or shell dispenser.

Servo features, shake motion, and buzzer output are disabled by default until their hardware is safely powered and calibrated.

## Website

The beach-themed site is served by the ESP32 at `http://192.168.4.1` on its open `Focus-Lock` Wi-Fi network. It displays the timer and state, shows the last scanned shell UID, edits that shell's duration, provides short developer demos, and plays a user-selected local music file.

## Main API

- `GET /status`: timer, pencil, RFID, sensor, servo, and selected-shell status.
- `GET /shells`: saved shell UID-to-duration mappings.
- `POST /shell`: save `{ "uid": "A1B2C3D4", "duration": 1800 }`.
- `POST /start`: developer demo duration in seconds.
- `POST /reset`: cancel and safely unlock.

Times are seconds. Up to 12 shell mappings and durations up to eight hours are supported.

## PlatformIO

```ini
platform = espressif32@6.12.0
board = esp32doit-devkit-v1
framework = arduino
```

Use PlatformIO **Upload** for firmware and **Upload Filesystem Image** for the website. The build script copies the browser assets into LittleFS automatically.

## Important files

```text
platformio.ini                      PlatformIO environment and libraries
firmware/src/main.cpp               ESP32 firmware
firmware/include/PinConfig.h        GPIO allocation
firmware/include/HardwareConfig.h   calibration and feature flags
firmware/scripts/sync_web.py        browser-to-LittleFS asset sync
index.html and src/                 website
HARDWARE_TRAINING.md                wiring and calibration procedure
PROJECT_LOG.md                      decisions and change history
```

## Power warning

SG90 servos require 4.8-6 V; regulated 5 V is the target. Do not power them from ESP32 3.3 V. The rectangular 9 V battery and breadboard regulator are not a dependable servo source. During development, power the ESP32 over USB and use an adequate regulated 5 V servo supply with its ground connected to ESP32 ground.

If the LCD1602 I2C backpack is powered at 5 V, verify that its SDA/SCL pull-ups do not expose ESP32 pins to 5 V; use 3.3 V operation or proper level shifting as required.

Apply the same caution to the TM1637 module: verify whether the actual board operates at 3.3 V or add suitable level shifting before using a 5 V-powered module with 3.3 V ESP32 GPIO.
