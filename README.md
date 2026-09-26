# Focus Lock website

The website is a beach-themed timer remote for the Focus Lock microcontroller. The ESP32 owns the sensors, servo, session state, and authoritative active-study clock. The browser only:

- displays remaining study time and progress;
- accepts a goal in minutes;
- sends a start command; and
- shows controller connectivity and the current session status;
- plays a locally selected study soundtrack.

The chosen music file stays on the device and is never uploaded. Choose an audio file, then use Play/Pause and Volume. Playback loops until paused or the page is closed. Browsers require the user to press Play; websites cannot start audio automatically without interaction.

## Run the website

Node.js 18 or newer is required. No packages need to be installed.

```sh
npm start
```

Open `http://localhost:4173`.

## Set the ESP32 address

Edit `src/config.js`:

```js
controllerUrl: "http://focus-lock.local"
```

Replace that value with the hostname or IP printed by the ESP32, for example `http://192.168.1.50`. If the ESP32 serves the website itself, use an empty string so requests go back to the same host.

## API required from the ESP32

### `GET /status`

The website requests this every 500 ms. Return JSON:

```json
{
  "phonePresent": true,
  "pencilPresent": false,
  "locked": true,
  "sessionActive": true,
  "activeTime": 120,
  "targetTime": 1800,
  "state": "LOCKED_STUDYING"
}
```

Times are seconds. `state` must be one of `IDLE`, `READY`, `LOCKED_PAUSED`, `LOCKED_STUDYING`, or `COMPLETE`.

### `POST /start`

The website sends JSON with the goal in seconds:

```json
{ "duration": 1800 }
```

Return the same status object as `GET /status` after locking and starting the session.

### Required headers

If the website is served from a computer while the ESP32 uses a different address, the ESP32 responses need:

```text
Access-Control-Allow-Origin: *
Access-Control-Allow-Headers: Content-Type
Access-Control-Allow-Methods: GET, POST, OPTIONS
```

The ESP32 must also answer `OPTIONS` preflight requests for `/start`. Serving the website directly from the ESP32 avoids cross-origin configuration.

## Current files

```text
index.html                         Timer UI
src/app.js                         Polling, rendering, and Start behavior
src/api/MicrocontrollerApi.js      ESP32 HTTP client
src/config.js                      Controller URL and polling interval
src/styles.css                     Timer UI styling
server.js                          Local static website server
PROJECT_LOG.md                     Complete change and decision history
```

The earlier simulation core remains in `src/core` and `src/hardware` as reference and is covered by tests, but it is no longer loaded by the website.
