# Focus Lock — Project Work Log

This document is the living record of work performed on **Focus Lock**. Update it whenever code, configuration, architecture, behavior, dependencies, documentation, or hardware integration changes.

## Project Goal

Focus Lock is a hackathon project built around an ESP32, two light sensors, and a servo-controlled phone drawer. A user chooses an active study-time goal, locks their phone, and earns access to it by studying. Active study time accumulates only while the pencil is picked up; putting the pencil down pauses the timer. Reaching the goal unlocks the phone.

The first milestone is a fully simulated software system. Real ESP32 sensor and servo behavior will replace the simulation later through a hardware abstraction layer.

## Required Core Flow

1. Phone inserted.
2. User starts a session.
3. Phone drawer locks.
4. Pencil picked up.
5. Active-study timer runs.
6. Pencil put down.
7. Timer pauses without resetting.
8. Pencil picked up again.
9. Timer continues from its accumulated value.
10. Active-study goal is reached.
11. Phone drawer unlocks.

## Scope

### In scope

- A clean, simple web frontend.
- Simulated phone, pencil, and servo controls.
- Session states: `IDLE`, `READY`, `LOCKED_PAUSED`, `LOCKED_STUDYING`, and `COMPLETE`.
- Timestamp-based active-time accounting.
- A fast 10–30 second developer demo mode.
- A hardware abstraction with the conceptual operations:
  - `isPhonePresent()`
  - `isPencilPresent()`
  - `lockPhone()`
  - `unlockPhone()`
- Mock API boundaries compatible with eventual ESP32 communication:
  - `GET /status`
  - `POST /start`
- Documentation for running, simulation, project structure, and later hardware integration.

### Explicitly out of scope

- RFID reader
- Crab mechanism
- Coin dispenser
- LED effects
- Authentication or user accounts
- Databases or cloud infrastructure
- Statistics or streaks
- Other bonus features

## Expected State Behavior

| Condition or event | Resulting state | Timer | Lock |
|---|---|---|---|
| No phone is present | `IDLE` | Stopped | Unlocked |
| Phone is inserted, no session started | `READY` | Stopped | Unlocked |
| Session starts with pencil down | `LOCKED_PAUSED` | Paused | Locked |
| Pencil is picked up during session | `LOCKED_STUDYING` | Running | Locked |
| Pencil is put down during session | `LOCKED_PAUSED` | Paused, accumulated time retained | Locked |
| Active time reaches target | `COMPLETE` | Stopped at goal | Unlocked |

## Planned User Interface

- Project name: Focus Lock
- Phone status: `DETECTED` / `NOT DETECTED`
- Pencil status: `PICKED UP` / `PUT DOWN`
- Lock status: `LOCKED` / `UNLOCKED`
- Study-duration input in minutes
- Start Session button
- Active study time
- Remaining study time
- Display status: `READY`, `STUDYING`, `PAUSED`, or `COMPLETE`
- Clearly separated **DEV / SIMULATION** controls for:
  - inserting/removing the phone
  - picking up/putting down the pencil
  - optionally locking/unlocking the simulated servo

## Architecture Notes

- Application/session logic must not directly depend on GPIO, sensors, or servo implementation details.
- Simulation and eventual ESP32 implementations should conform to the same hardware interface.
- Timer correctness must be based on timestamps. Visual refresh intervals may update the screen but must not be the authoritative elapsed-time source.
- Networking should stay minimal. Mock API behavior should preserve a clean seam for a later ESP32-backed implementation.
- The implementation should favor a small, readable hackathon stack and avoid unnecessary abstractions.

## Repository Baseline

Recorded on **2026-09-25**:

- Workspace inspected at `C:\Phone Lock`.
- No existing project files were found.
- The project specification was supplied as an attached text file.
- No application architecture or framework has been selected or created yet.

## Change Log

### 2026-09-26 — Final interaction model corrected and implemented

**Corrected requirements**

- The crab claws are stationary. The pencil rests across them; removing the pencil starts or resumes active study time, and returning it pauses.
- RFID tags are embedded in shells. Each immutable tag UID maps to an editable duration stored in ESP32 nonvolatile memory.
- The website scans/displays the active shell and changes its associated duration; RFID is not a reward or unlock mechanism.
- Reaching zero unlocks the phone drawer.
- TM1637 is the physical countdown display only. LCD1602 displays the crab eyes.
- There is no reward or shell dispenser.
- The second SG90 sways the crab laterally while studying.
- A buzzer indicates session start and completion. There is still no sensor inside the phone cabinet.
- SG90 supply must remain within 4.8-6 V, with regulated 5 V preferred.

**Implementation changes**

- Reworked the ESP32 state flow to `WAITING_FOR_SHELL`, `ARMED`, `LOCKED_PAUSED`, `LOCKED_STUDYING`, and `COMPLETE`.
- Added RC522 scanning and persistent storage for up to 12 UID-duration mappings.
- Added `GET /shells` and `POST /shell` and retained developer start/reset routes.
- Reworked the website into a scanned-shell duration editor.
- Changed sensor logic from claw movement to stationary pencil presence/removal.
- Added TM1637 countdown output, an LCD1602 I2C eye-display driver, passive-buzzer tones, and safely disabled second-servo sway behavior.
- Assigned GPIO17 to the buzzer and GPIO25 to the shake servo.
- Rewrote `README.md` and `HARDWARE_TRAINING.md` around the corrected design.

**Safety defaults**

- Lock servo, shake servo, and buzzer remain disabled until their physical calibration/type checks are complete.
- LCD1602 defaults to I2C address `0x27`; power and pull-up voltage must be verified.

**Verification**

- Corrected firmware compiles for `esp32doit-devkit-v1` with RC522 and TM1637 libraries.
- Build uses 46,360 bytes RAM (14.1%) and 877,705 bytes application flash (67.0%) before the final browser/documentation-only changes.

### 2026-09-26 — Safe ESP32 firmware base implemented

**Changes**

- Added the Arduino firmware entry point with dual VL53L0X XSHUT/address initialization, filtered claw state, timestamp-based session timing, SG90 hooks, safe startup behavior, Wi-Fi access-point mode, HTTP API routes, and LittleFS website serving.
- Added `HardwareConfig.h`; servo output defaults to disabled until physical calibration is complete.
- Added build-time synchronization of browser assets into the firmware filesystem image.
- Added 10/20/30-second website demo buttons and sensor-readiness handling.
- Added `HARDWARE_TRAINING.md` with wiring, sensor threshold training, safe servo calibration, filesystem upload, and full-flow validation instructions.
- Pinned PlatformIO Espressif32 to `6.12.0` because the initially resolved `7.1.3` Arduino package lacked its required board variants.
- Removed deferred RFID and teammate-owned TM1637 libraries from the base dependency list; each feature can add its exact library when integrated.

**Verification**

- PlatformIO firmware build passes for `esp32doit-devkit-v1` using Arduino.
- Firmware uses 45,736 bytes RAM (14.0%) and 854,085 bytes flash (65.2%).
- LittleFS image builds successfully with the complete website module tree.
- Hardware upload and physical behavior remain untested until the board and safely powered components are connected.

### 2026-09-26 — Claw sensing and power plan corrected

**New clarifications**

- There is no phone sensor inside the box.
- Both VL53L0X sensors are mounted at the crab claws and measure claw position. When the pencil is lifted and the claws rise, active study timing begins; returning the pencil/claws pauses it.
- Starting a session no longer requires a `phonePresent` reading. Pressing Start commands the lock and begins the session in studying or paused state based on claw position.
- The TM1637 forms the crab eyes. A teammate owns its implementation and will push it separately; preserve GPIO14/GPIO13 and integrate rather than overwrite their work.
- Two SG90 servos are available, but the second servo shell dispenser remains a later phase.
- The photographed breadboard supply is an ELEGOO Power MB V2 intended to receive the available 9 V battery through its barrel input.

**Power decision**

- Do not rely on a rectangular 9 V battery for SG90 servo power. Use ESP32 USB power during development and a separate regulated 5 V servo supply with common ground. The 9 V/breadboard module may be used only for low-current experiments after verifying jumper voltages and polarity.

**Tooling**

- Installed the official PlatformIO IDE VS Code extension v3.3.4 and its Microsoft C++ tools dependency.

**Files changed**

- Renamed the two VL53L0X definitions in `PinConfig.h` to left/right claw sensors.
- Removed the website's `phonePresent` requirement for enabling Start.
- Updated `README.md` and `PROJECT_LOG.md`.

### 2026-09-26 — Two-sensor demo architecture finalized

**Hardware evidence and decisions**

- Photos confirm a classic 30-pin ESP32/ESP-WROOM-32 DevKit-style board compatible with the selected PlatformIO target. The photographed connector appears to be Micro-USB rather than USB-C; this does not change firmware targeting.
- Reallocated the two available VL53L0X sensors: one detects the pencil and one detects the phone. This supersedes the earlier two-sided pencil detection and proposed third sensor.
- Pencil presence will rely on careful placement and debounce/filtering from one distance sensor.
- Two SG90 servos are available. SG90 #1 is the phone lock on GPIO26. SG90 #2 and GPIO25 are reserved for a future shell dispenser; dispenser behavior is deferred.
- When the dispenser phase is implemented, the simplest RFID policy is selected: any readable tag may claim one shell only after a completed session, and the reward is marked claimed to prevent repeats.
- RFID never overrides an active session or unlocks the phone.
- No buzzer is included.
- The demo access point will be open and named `Focus-Lock`; a password can be added later if interference or unauthorized control becomes a concern.
- The website may accept flexible durations. The four-digit display uses `MM:SS` under 100 minutes and may use `HH:MM` for longer goals; quick 10/20/30-second modes support demonstrations.
- On session start, timing begins immediately if the pencil is already picked up.
- Loss of phone detection during an active session does not stop or unlock it.
- Startup behavior remains fail-safe unlock.
- Exact lock/unlock and future dispense angles require physical calibration.

**Files changed**

- Updated `firmware/include/PinConfig.h` to use two sensors at addresses `0x30` and `0x31`, freeing GPIO16.
- Updated `README.md` and `PROJECT_LOG.md` with the finalized demo architecture.

### 2026-09-26 — PlatformIO target selected

**Confirmed build environment**

- PlatformIO project environment: `esp32doit-devkit-v1`.
- Framework: Arduino.
- Target: classic 30-pin ESP32/ESP-WROOM-32 DevKit V1 with 3.3 V GPIO logic.
- Firmware source is isolated under `firmware/` so it does not conflict with the existing browser JavaScript under `src/`.
- LittleFS is selected for eventually serving the website directly from the ESP32 access point.

**Changes**

- Added `platformio.ini` with the selected board/framework and planned hardware libraries.
- Added `firmware/include/PinConfig.h` with confirmed pins, proposed third VL53L0X XSHUT on GPIO16, runtime I2C addresses, and GPIO25 reserved for the unselected dispenser actuator.

**Tooling note**

- PlatformIO CLI is not currently installed or available on this machine's PATH, so firmware compilation cannot yet be verified locally.

### 2026-09-26 — Hardware behavior decisions clarified

**Confirmed decisions**

- Controller: generic 30-pin classic ESP32/ESP-WROOM-32 DevKit V1, built as `DOIT ESP32 DEVKIT V1` or equivalent `ESP32 Dev Module`, using 3.3 V GPIO logic.
- Phone presence will use a distance sensor rather than RFID.
- Both existing VL53L0X sensors must detect the pencil for it to count as put down.
- Because those two sensors are allocated to the pencil, continuous phone detection requires a third distance sensor or a reallocation of an existing sensor.
- RC522 purpose: trigger the crab's shell-dispensing action, not phone detection or session override.
- No buzzer will be used; remove GPIO25 buzzer behavior from the firmware plan.
- If the pencil is already picked up when a session starts, active time begins immediately.
- Loss of phone-presence detection during a locked session does not stop or unlock the session; it should continue and may expose a warning status.
- On ESP32 startup or power restoration, unlock the phone for prototype safety.
- RFID does not override or unlock an active study session.
- The ESP32 will create its own Wi-Fi access point and serve the website itself. This avoids dependence on venue Wi-Fi and removes cross-origin API configuration.
- Demo timing will support short second-based sessions; the physical four-digit display will use `MM:SS` with a practical maximum of `99:59`.

**Still unresolved**

- Exact module photos/pin labels have not yet been supplied.
- Servo model, external 5 V supply rating, and mechanical lock design are unknown.
- TM1637 operation at 3.3 V still needs testing; 5 V operation may require level shifting.
- Shell dispenser mechanism and its actuator/driver are not specified.
- The third phone-distance sensor and its XSHUT pin must be added to the wiring plan if it is another VL53L0X.

### 2026-09-26 — Hardware architecture revised for selected parts

**Selected hardware and pins**

- RC522 RFID over SPI: CS 5, SCK 18, MOSI 23, MISO 19, RST 27.
- Two VL53L0X sensors on I2C SDA 21/SCL 22, with XSHUT on 32 and 33.
- TM1637 display on CLK 14/DIO 13.
- Externally powered servo signal on GPIO 26.
- Passive buzzer signal on GPIO 25.

**Architecture decisions and cautions**

- The ESP32 remains authoritative for sensors, session state, active-time accounting, display, buzzer, and servo safety; the website remains a remote control/status view.
- The two identical VL53L0X devices require sequential XSHUT initialization and different runtime I2C addresses after every boot.
- The servo uses a separate regulated 5 V supply with its ground joined to ESP32 ground.
- Prefer powering the TM1637 module at 3.3 V if the actual module operates reliably; otherwise add bidirectional 3.3/5 V level shifting rather than exposing ESP32 pins to 5 V pull-ups.
- A bare piezo buzzer can be GPIO-driven; a magnetic/high-current buzzer requires a transistor driver.
- The listed pair of distance sensors is allocated to pencil/claw sensing, leaving continuous phone-presence detection unresolved. RFID can identify a tag but should not automatically be assumed to provide reliable continuous presence near a phone without physical testing.

**Recommended integration order**

1. ESP32 upload and serial diagnostics.
2. Shared I2C bus and dual-sensor address assignment.
3. Pencil-presence decision from both distance readings.
4. TM1637 countdown display.
5. Passive buzzer tones.
6. Separately powered unloaded servo and angle calibration.
7. RC522 read testing and phone-presence strategy validation.
8. State machine and timestamp timer.
9. HTTP status/start API and website connection.
10. Enclosure installation, recalibration, and repeated safety tests.

### 2026-09-25 — Beach theme, countdown, and local music

**Request**

Give the website a simple beach/crab theme, add background music during study, count down, and animate the waves slowly.

**Changes**

- Changed the main clock to show `targetTime - activeTime` from ESP32 status responses.
- Added a soft sky, sand, ocean, clouds, crab accents, and slowly moving CSS waves.
- Added a private local-audio picker with looping play/pause and volume controls.
- Added reduced-motion support that stops decorative animation when requested by the operating system.
- Kept the page free of image and audio dependencies; visuals are CSS and the user supplies their own music.

**Files changed**

- Updated `index.html`, `src/app.js`, `src/styles.css`, `README.md`, and `PROJECT_LOG.md`.

**Known limitations / follow-up**

- Browsers require an explicit user action before audio playback; a selected soundtrack cannot autoplay on session start unless the user has already interacted with playback.

### 2026-09-25 — Website changed to microcontroller timer remote

**Request**

Make the website contain only the time controls and communicate with the microcontroller.

**Approach / decisions**

- Moved ownership of sensor readings, lock state, session state, and timer calculation to the ESP32 contract.
- Reduced the browser to a thin REST client that polls status and sends session starts.
- Kept the controller URL in one configuration file.

**Changes**

- Removed all simulation and hardware controls from the webpage.
- Simplified the interface to active time, progress, duration, Start, status, and connectivity.
- Added `MicrocontrollerApi` with three-second timeouts and useful offline errors.
- Added 500 ms polling of `GET /status` and `POST /start` with duration in seconds.
- Start is disabled while disconnected, during an active session, or when the controller reports no phone.

**Files changed**

- Replaced `index.html`, `src/app.js`, and `src/styles.css` with the thin-controller UI.
- Added `src/api/MicrocontrollerApi.js`.
- Updated `src/config.js`, `README.md`, and `PROJECT_LOG.md`.

**Known limitations / follow-up**

- ESP32 firmware must implement the documented routes and CORS headers.
- The configured hostname must resolve on the user's local network.

**Verification**

- Browser application and API client pass Node syntax checks.
- All 5 retained state-machine reference tests pass.

### 2026-09-25 — Simulated application implemented

**Request**

Code everything possible before hardware is connected.

**Approach / decisions**

- Chose dependency-free HTML, CSS, and JavaScript with a tiny Node static server. This minimizes setup and keeps the hackathon prototype inspectable.
- Made `SessionController` the authoritative state and timing layer.
- Isolated all sensor and servo operations behind `HardwareAdapter`.
- Used elapsed timestamps rather than interval counts for timer accuracy.

**Changes**

- Built the responsive Focus Lock dashboard and status display.
- Added phone and pencil simulation controls plus 10, 20, and 30-second demos.
- Implemented all five required session states, pause/resume accumulation, completion, reset, and automatic unlock.
- Added a mock API facade and a prepared ESP32 HTTP adapter.
- Added a dependency-free static server and automated core logic tests.
- Added setup, architecture, simulation, API, and ESP32 migration documentation.

**Files changed**

- Added `index.html`, `server.js`, `package.json`, `.gitignore`, and `README.md`.
- Added the application under `src/` and automated tests under `test/`.
- Updated `PROJECT_LOG.md`.

**Verification**

- All 5 automated tests pass: IDLE/READY, start/lock, pause/resume accumulation, completion/unlock, invalid start, and simulated phone removal.
- The test command runs the Node test file directly because this workspace blocks the test runner's optional child-process isolation.
- Local-server smoke check passed: `/` and `/src/app.js` both returned HTTP 200, and the page contained the expected Focus Lock title.

**Known limitations / follow-up**

- Real ESP32 firmware, GPIO pin assignments, light thresholds, servo angles, network address, and reconnection behavior require connected hardware.
- Current API is an in-browser facade rather than an HTTP backend; its method and payload shape mirrors the planned endpoints.

### 2026-09-25 — Work log initialized

**Request**

Create a Markdown file that stores everything done and all project changes.

**Changes**

- Added `PROJECT_LOG.md` as the project’s living work and change record.
- Captured the supplied Focus Lock goal, core flow, scope boundaries, required states, planned UI, API direction, timer constraints, and hardware-separation requirements.
- Recorded the empty-repository baseline.

**Files changed**

- Added `PROJECT_LOG.md`.

**Verification**

- Confirmed the repository contained no files before this document was added.
- No build or automated tests were available or applicable.

## Decision Log

| Date | Decision | Reason |
|---|---|---|
| 2026-09-25 | Use `PROJECT_LOG.md` as the canonical ongoing work record. | Keeps implementation history, decisions, validation, and pending work in one visible place. |
| 2026-09-25 | Defer application stack selection until implementation begins. | The current request only asks for the tracking document; recording a framework decision before implementation would be premature. |
| 2026-09-25 | Use dependency-free browser modules and a Node static server. | Keeps setup, debugging, and handoff simple while providing clean modules and tests. |
| 2026-09-25 | Treat `pencilPresent: true` as pencil put down. | A resting pencil covers the light sensor; picking it up means it is absent and active study begins. |
| 2026-09-25 | Reset an active simulated session if the phone is removed. | Preserves the invariant that no detected phone means `IDLE`; real locked hardware should physically prevent this case. |

## Pending Work

- Connect the physical sensors and determine reliable light thresholds.
- Define ESP32 GPIO pins and locked/unlocked servo angles.
- Implement ESP32 firmware HTTP endpoints.
- Add polling, connection-state feedback, and retry handling for ESP32 mode.
- Validate servo fail-safe behavior on power loss or network failure.

## Entry Template for Future Changes

Copy this section for every meaningful unit of work:

```md
### YYYY-MM-DD — Short change title

**Request**

What was requested or what problem was addressed.

**Approach / decisions**

- Important implementation choices and their reasons.

**Changes**

- Concrete behavior, code, configuration, dependency, or documentation changes.

**Files changed**

- `path/to/file` — concise description.

**Verification**

- Commands/tests performed and their outcomes.

**Known limitations / follow-up**

- Anything intentionally incomplete or worth revisiting.
```
