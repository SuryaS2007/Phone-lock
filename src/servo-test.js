import { MicrocontrollerApi } from "./api/MicrocontrollerApi.js";
import { config } from "./config.js";

const api = new MicrocontrollerApi(config.controllerUrl || window.location.origin);
const elements = {
  connection: document.querySelector("#connection"),
  range: document.querySelector("#angle-range"),
  number: document.querySelector("#angle-number"),
  output: document.querySelector("#angle-output"),
  move: document.querySelector("#move-servo"),
  presets: [...document.querySelectorAll("[data-angle]")],
  saveUnlock: document.querySelector("#save-unlock"),
  saveLock: document.querySelector("#save-lock"),
  saved: document.querySelector("#saved-angles"),
  outer: document.querySelector("#outer-angle"),
  inner: document.querySelector("#inner-angle"),
  repetitions: document.querySelector("#repetitions"),
  hold: document.querySelector("#hold-ms"),
  run: document.querySelector("#run-sequence"),
  detach: document.querySelector("#detach-servo"),
  status: document.querySelector("#servo-status")
};

let connected = false;
let requestPending = false;

function clampAngle(value) {
  return Math.max(0, Math.min(180, Math.round(Number(value) || 0)));
}

function setSelectedAngle(value) {
  const angle = clampAngle(value);
  elements.range.value = angle;
  elements.number.value = angle;
  elements.output.value = `${angle}°`;
}

function setConnection(value) {
  connected = value;
  elements.connection.classList.toggle("offline", !value);
  elements.connection.classList.toggle("online", value);
  elements.connection.lastChild.textContent = value ? " Connected" : " Offline";
}

function setControlsDisabled(disabled, sequenceActive = false) {
  const shouldDisable = disabled || !connected || sequenceActive;
  [elements.range, elements.number, elements.move, elements.saveUnlock, elements.saveLock,
    elements.outer, elements.inner, elements.repetitions, elements.hold, elements.run,
    ...elements.presets].forEach((element) => { element.disabled = shouldDisable; });
}

function render(data) {
  elements.saved.textContent = `Saved: unlocked ${data.unlockAngle}°, locked ${data.lockAngle}°`;
  const progress = data.sequenceActive
    ? `Sequence running: ${data.completedRepetitions}/${data.totalRepetitions} complete`
    : data.attached
      ? `Servo attached${data.currentAngle >= 0 ? ` at ${data.currentAngle}°` : ""}`
      : `Servo detached${data.currentAngle >= 0 ? `; last command ${data.currentAngle}°` : ""}`;
  elements.status.textContent = data.available ? progress : "Servo tools are unavailable during a study session.";
  setControlsDisabled(requestPending || !data.available, data.sequenceActive);
  elements.detach.disabled = requestPending || (!data.attached && !data.sequenceActive);
}

async function refresh() {
  try {
    const data = await api.getServoStatus();
    setConnection(true);
    render(data);
  } catch (error) {
    setConnection(false);
    setControlsDisabled(true);
    elements.detach.disabled = true;
    elements.status.textContent = error.message;
  }
}

async function perform(message, action) {
  if (requestPending) return;
  requestPending = true;
  setControlsDisabled(true);
  elements.status.textContent = message;
  try { render(await action()); }
  catch (error) { elements.status.textContent = error.message; }
  finally { requestPending = false; await refresh(); }
}

elements.range.addEventListener("input", () => setSelectedAngle(elements.range.value));
elements.number.addEventListener("input", () => setSelectedAngle(elements.number.value));
elements.move.addEventListener("click", () => {
  const angle = clampAngle(elements.number.value);
  perform(`Moving directly to ${angle}°…`, () => api.moveServo(angle));
});

elements.presets.forEach((button) => {
  button.addEventListener("click", () => {
    const angle = clampAngle(button.dataset.angle);
    setSelectedAngle(angle);
    perform(`Moving directly to ${angle}°…`, () => api.moveServo(angle));
  });
});

elements.saveUnlock.addEventListener("click", () => {
  const angle = clampAngle(elements.number.value);
  perform(`Saving ${angle}° as unlocked…`, () => api.saveServoAngle("unlock", angle));
});

elements.saveLock.addEventListener("click", () => {
  const angle = clampAngle(elements.number.value);
  perform(`Saving ${angle}° as locked…`, () => api.saveServoAngle("lock", angle));
});

elements.run.addEventListener("click", () => {
  const outerAngle = clampAngle(elements.outer.value);
  const innerAngle = clampAngle(elements.inner.value);
  const repetitions = Math.max(1, Math.min(10, Math.round(Number(elements.repetitions.value) || 1)));
  const holdMs = Math.max(100, Math.min(5000, Math.round(Number(elements.hold.value) || 700)));
  perform("Starting direct-position sequence…", () => api.runServoSequence({ outerAngle, innerAngle, repetitions, holdMs }));
});

elements.detach.addEventListener("click", () => {
  perform("Stopping and detaching…", () => api.detachServo());
});

setSelectedAngle(90);
refresh();
setInterval(refresh, config.statusPollMs);
