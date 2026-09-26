# Focus Lock hardware training

Follow these stages in order. Do not connect the locking linkage during the first servo test.

## 1. Prepare safe power

For development:

```text
Computer USB -> ESP32 USB port
ESP32 3V3    -> both VL53L0X VIN pins
ESP32 GND    -> both VL53L0X GND pins
Regulated 5V -> SG90 red wire
5V ground    -> SG90 brown/black wire AND ESP32 GND
GPIO26       -> SG90 orange/yellow signal wire
```

Do not use the rectangular 9 V battery and breadboard regulator as the dependable SG90 supply. Obtain a regulated 5 V supply or USB power source capable of handling servo current. Do not connect its 5 V output to ESP32 `3V3`.

Leave the servo power disconnected during sensor training.

## 2. Wire the two claw sensors

```text
Both SDA    -> GPIO21
Both SCL    -> GPIO22
Both VIN    -> 3V3
Both GND    -> GND
Left XSHUT  -> GPIO32
Right XSHUT -> GPIO33
```

The firmware holds both sensors off, starts the left sensor at `0x30`, and then starts the right sensor at `0x31`.

## 3. Build and upload the safe base

1. Restart VS Code after installing PlatformIO.
2. Open this repository folder.
3. Open the PlatformIO sidebar.
4. Under `esp32doit-devkit-v1`, run **Build**.
5. Connect the ESP32 over USB and run **Upload**.
6. Run **Upload Filesystem Image** so the website is copied into LittleFS.
7. Open **Monitor** at 115200 baud.

The first boot should say `SAFE MODE: Servo output disabled`. This is intentional.

## 4. Train the claw distances

With `SENSOR_DIAGNOSTICS = true`, Serial Monitor prints values such as:

```text
CLAW_RAW left=42 right=45 lifted=false
```

Record around ten stable readings for each condition:

| Condition | Left range | Right range |
|---|---:|---:|
| Pencil resting / claws down | | |
| Pencil lifted / claws up | | |

For each sensor, choose a threshold halfway between its average down and up values.

Example only:

```text
Left down average:  40 mm
Left up average:   120 mm
Threshold:          80 mm
```

Edit `firmware/include/HardwareConfig.h`:

```cpp
constexpr unsigned int LEFT_CLAW_THRESHOLD_MM = 80;
constexpr unsigned int RIGHT_CLAW_THRESHOLD_MM = 80;
```

If lifted values are higher, keep `CLAW_LIFTED_WHEN_DISTANCE_GREATER = true`. If lifted values are lower, change it to `false`.

Rebuild and upload. Confirm Serial Monitor shows `LIFTED / STUDYING` only when both claws are lifted and `DOWN / PAUSED` when either claw is down.

## 5. Test timing before enabling the servo

1. Connect a phone or laptop to the open Wi-Fi network `Focus-Lock`.
2. Open `http://192.168.4.1`.
3. Select the 10-second quick demo.
4. Keep the claws down and confirm the timer remains paused.
5. Lift both claws and confirm the timer counts down.
6. Lower either claw and confirm it pauses.
7. Lift both again and confirm it reaches zero.

The website will say the servo is in safe setup mode. This is expected.

## 6. Calibrate the SG90 without the mechanism

Disconnect power before changing wiring. Remove the servo horn or disconnect the lock linkage.

In `HardwareConfig.h`, set:

```cpp
constexpr bool SERVO_ENABLED = false;
constexpr bool SERVO_CALIBRATION_MODE = true;
constexpr int SERVO_CALIBRATION_START_ANGLE = 90;
```

Connect the servo to a proper regulated 5 V supply with common ground, rebuild, upload, and open Serial Monitor. Send `+` to move 5 degrees higher or `-` to move 5 degrees lower.

Find unlock and lock positions without forcing the mechanism. Stop if the servo buzzes, stalls, heats up, or presses against a hard stop.

Record the working angles:

```cpp
constexpr int SERVO_UNLOCK_ANGLE = ...;
constexpr int SERVO_LOCK_ANGLE = ...;
```

Then set:

```cpp
constexpr bool SERVO_CALIBRATION_MODE = false;
constexpr bool SERVO_ENABLED = true;
```

Reattach the linkage while the servo is at the known unlock angle.

## 7. Run the complete demo

1. Restart the ESP32 and confirm it moves to unlock.
2. Connect to `Focus-Lock` and open `http://192.168.4.1`.
3. Start a 10-second demo; confirm the drawer locks.
4. Lift both claws; confirm the countdown runs.
5. Lower either claw; confirm it pauses.
6. Lift both claws again; confirm it finishes and unlocks.
7. Repeat at least ten times before placing a valuable phone inside.

## 8. Teammate display integration

The TM1637 crab-eye display owns GPIO14 (`CLK`) and GPIO13 (`DIO`). Pull your teammate's branch before integrating their display code. Do not duplicate or overwrite their implementation. The timer values they need are `activeTimeMs()`, `targetTimeMs`, and `sessionState` in `firmware/src/main.cpp`.
