# Focus Lock hardware training

Work through these stages in order. Keep `SERVO_ENABLED`, `SHAKE_SERVO_ENABLED`, and `BUZZER_ENABLED` false until each component is tested safely.

## 1. Power safely

```text
Computer USB -> ESP32
ESP32 3V3 -> VL53L0X and RC522 power
Regulated 5V -> SG90 red wires
5V supply ground -> servo grounds AND ESP32 GND
```

Do not use the rectangular 9 V battery as the dependable servo source. SG90 voltage must remain within 4.8-6 V; use regulated 5 V.

## 2. Wire the pencil sensors

```text
Both SDA    -> GPIO21
Both SCL    -> GPIO22
Both VIN    -> 3V3
Both GND    -> GND
Left XSHUT  -> GPIO32
Right XSHUT -> GPIO33
```

The claws do not move. Mount one sensor under each side of the pencil rest. Firmware assigns the sensors I2C addresses `0x30` and `0x31` after every boot.

## 3. Upload the safe base

1. Build and upload the `esp32doit-devkit-v1` environment.
2. Run **Upload Filesystem Image**.
3. Open Serial Monitor at 115200 baud.
4. Confirm both pencil sensors and RC522 initialize.

## 4. Calibrate pencil presence

Serial Monitor prints:

```text
PENCIL_RAW left=42 right=45 removed=false
```

Record ten readings with the pencil resting and ten with it removed. Put the halfway values in `HardwareConfig.h`:

```cpp
LEFT_PENCIL_THRESHOLD_MM = ...;
RIGHT_PENCIL_THRESHOLD_MM = ...;
```

If readings are lower when the pencil is present, keep `PENCIL_PRESENT_WHEN_DISTANCE_LESS = true`; otherwise change it to `false`.

The firmware considers the pencil removed only when both sensors report it absent for at least 350 ms. Returning it over either sensor pauses the timer.

## 5. Train RFID shell durations

Wire the RC522 using the pins in `PinConfig.h`, restart, and connect to the open `Focus-Lock` Wi-Fi network.

1. Open `http://192.168.4.1`.
2. Scan one shell tag.
3. Confirm its UID appears on the website.
4. Enter a duration and select **Save shell**.
5. Scan another shell and give it a different duration.
6. Restart the ESP32 and verify each saved duration remains.

The tag UID is not changed; only its stored duration association changes.

## 6. Test the timer without servos

1. Scan a configured shell or choose a 10-second demo.
2. Rest the pencil across the claws.
3. Lift the pencil and confirm the countdown begins.
4. Return it and confirm the countdown pauses.
5. Lift it again and confirm the timer reaches zero.
6. Confirm the TM1637 shows remaining `MM:SS`.

Verify the TM1637 module's logic voltage before connection. Prefer 3.3 V operation if the module is reliable there; otherwise use appropriate level shifting for its CLK/DIO signals.

## 7. Calibrate the lock SG90

For an unloaded bench test, `SERVO_CYCLE_TEST_MODE` slowly moves the lock servo on GPIO26 from 90 degrees to 0 degrees and back to 90 degrees three times, then detaches the servo signal. Physical clockwise/counterclockwise direction depends on servo mounting. Keep this mode disabled during normal operation and never run it against a mechanical hard stop.

`SERVO_SWIFT_TEST_MODE` sends direct positions without intermediate steps. The current physical 270 -> 180 -> 270 reference maps to servo commands 90 -> 0 -> 90. All executed tests are recorded in `SERVO_TEST_RUNS.md`.

For normal calibration, connect to the `Focus-Lock` Wi-Fi network and open `http://192.168.4.1/servo-test.html`. The page can move to a direct angle, run repeated position sequences, stop and detach the servo, and save the selected angle as the locked or unlocked position. Saved angles persist in ESP32 nonvolatile memory. Servo controls are rejected while a study session is armed or running.

Remove the horn or disconnect the linkage. Use a regulated 5 V supply with common ground. Set:

```cpp
SERVO_ENABLED = false;
SERVO_CALIBRATION_MODE = true;
```

Upload and send `+` or `-` in Serial Monitor to move five degrees at a time. Record safe unlock and lock angles, then set:

```cpp
SERVO_UNLOCK_ANGLE = ...;
SERVO_LOCK_ANGLE = ...;
SERVO_CALIBRATION_MODE = false;
SERVO_ENABLED = true;
```

Stop immediately if the servo stalls, buzzes continuously, heats up, or pushes against a hard stop.

## 8. Add buzzer and shake servo

The buzzer code assumes a passive buzzer on GPIO17. After verifying its type and wiring, set `BUZZER_ENABLED = true`.

The second SG90 uses GPIO25. Calibrate safe left, center, and right angles before setting `SHAKE_SERVO_ENABLED = true`. When enabled, it sways only during `LOCKED_STUDYING` and centers while paused or complete.

## 9. LCD1602 eyes

The base assumes an I2C-backpack LCD1602 at address `0x27` on GPIO21/GPIO22. If it does not respond, run an I2C scan; common alternatives include `0x3F`. Verify safe I2C logic voltage before using a 5 V backpack.

Connect the four I2C-backpack pins as follows:

| LCD1602 backpack | ESP32 |
| --- | --- |
| GND | GND |
| VCC | Safe supply for the specific backpack; verify its logic voltage |
| SDA | GPIO21 (shared with the VL53L0X sensors) |
| SCL | GPIO22 (shared with the VL53L0X sensors) |

The integrated firmware now animates center, left, right, blink, surprised, happy, and sleeping eyes. The expressions respond to the Focus Lock state: selecting a shell shows surprised eyes, a paused timer sleeps, active study uses the calm look/blink loop, and completion shows happy eyes.

Do not connect the LCD using the earlier six-wire prototype assignment `(23, 22, 21, 19, 18, 5)`. Those pins conflict with the RC522 and VL53L0X hardware. The main firmware uses the I2C backpack so the LCD shares only GPIO21/GPIO22.

## 10. Acceptance test

Repeat at least ten times:

```text
Scan shell -> duration loads -> pencil rests -> pencil lifts
-> lock and start sound -> countdown runs -> pencil returns
-> countdown pauses -> pencil lifts -> countdown completes
-> unlock and completion sound
```

Test with a nonvaluable object before placing a phone inside, and retain a manual release.
