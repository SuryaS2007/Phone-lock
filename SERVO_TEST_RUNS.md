# Focus Lock servo test runs

This file records physical lock-servo tests performed on GPIO26. The servo uses an external regulated 5 V supply with a ground shared with the ESP32.

## Angle notation

The SG90 control command accepts angles from 0 to 180 degrees. For the current mechanical orientation, the team's physical reference labels map as follows:

| Physical reference | Servo command |
| --- | ---: |
| 270 degrees | 90 degrees |
| 180 degrees | 0 degrees |

Physical clockwise/counterclockwise direction depends on which side of the servo is viewed and how its horn is installed.

## 2026-09-26 — Connection and small-movement test

- Board detected as a CP2102 device on COM4 after installing its Windows driver.
- Servo signal: GPIO26.
- Commands: 90 -> 95 -> 100 -> 105 -> 110 -> 105 -> 100 -> 95 -> 90 degrees.
- Result: upload and commands succeeded; user confirmed that the servo moved.
- Firmware was restored with calibration mode disabled.

## 2026-09-26 — Slow three-cycle test

- Commands: 90 -> 0 -> 90 degrees.
- Repetitions: three.
- Motion: smooth 1-degree steps with a 15 ms delay per step.
- End state: 90 degrees, followed by detaching the servo signal.
- Firmware upload completed and automatic test mode was disabled afterward.

## 2026-09-26 — Swift physical 270/180/270 test

- Physical sequence: 270 -> 180 -> 270 degrees.
- Servo command sequence: 90 -> 0 -> 90 degrees.
- Motion: direct position commands with no intermediate 1-degree steps.
- Result: firmware built and uploaded successfully; sequence executed once. Physical movement confirmation is pending from the user.
- Safe restoration: automatic swift test mode was disabled after execution and normal firmware was re-uploaded.

## Reusable browser test controller

- Page: `http://192.168.4.1/servo-test.html` while connected to `Focus-Lock` Wi-Fi.
- Supports direct positions, presets, repeated direct-position sequences, and emergency detach.
- Supports saving calibrated locked and unlocked angles in ESP32 nonvolatile memory.
- Test controls are blocked while a study session is armed or running.
- The page is controlled by `DEVELOPER_TOOLS_ENABLED` in `HardwareConfig.h`.

## 2026-09-26 — Direct firmware fallback test

- Reason: browser pages still reported offline, so the website was bypassed.
- Physical sequence: 270 -> 180 -> 270 degrees.
- Servo command sequence: 90 -> 0 -> 90 degrees on GPIO26.
- Motion: direct commands with no stepping.
- Firmware upload completed, the sequence ran once, and automatic test mode was disabled afterward.
