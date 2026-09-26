import { MicrocontrollerApi } from "./api/MicrocontrollerApi.js";
import { config } from "./config.js";

const api = new MicrocontrollerApi(config.controllerUrl || window.location.origin);
const elements = {
  connection: document.querySelector("#connection"),
  time: document.querySelector("#time"),
  status: document.querySelector("#status"),
  progress: document.querySelector(".progress"),
  fill: document.querySelector("#progress-fill"),
  form: document.querySelector("#shell-form"),
  uid: document.querySelector("#shell-uid"),
  duration: document.querySelector("#shell-duration"),
  save: document.querySelector("#save-shell"),
  message: document.querySelector("#message"),
  musicFile: document.querySelector("#music-file"),
  musicToggle: document.querySelector("#music-toggle"),
  volume: document.querySelector("#volume"),
  audio: document.querySelector("#audio"),
  trackName: document.querySelector("#track-name"),
  quickDemo: [...document.querySelectorAll("[data-demo-seconds]")]
};

let connected = false;
let requestPending = false;
let musicUrl = null;
let displayedUid = "";

function formatTime(seconds) {
  const value = Math.max(0, Math.ceil(Number(seconds) || 0));
  const hours = Math.floor(value / 3600);
  const minutes = Math.floor((value % 3600) / 60);
  const remainder = value % 60;
  return hours > 0
    ? `${String(hours).padStart(2, "0")}:${String(minutes).padStart(2, "0")}:${String(remainder).padStart(2, "0")}`
    : `${String(minutes).padStart(2, "0")}:${String(remainder).padStart(2, "0")}`;
}

function labelFor(state) {
  return ({
    WAITING_FOR_SHELL: "Scan an RFID shell",
    ARMED: "Shell loaded — lift pencil to begin",
    LOCKED_PAUSED: "Paused — pencil is resting",
    LOCKED_STUDYING: "Studying",
    COMPLETE: "Complete — phone unlocked"
  })[state] || "Waiting for controller";
}

function setConnection(value) {
  connected = value;
  elements.connection.classList.toggle("offline", !value);
  elements.connection.classList.toggle("online", value);
  elements.connection.lastChild.textContent = value ? " Connected" : " Offline";
  if (!value) disableControls(true);
}

function disableControls(disabled) {
  elements.save.disabled = disabled || !elements.uid.value;
  elements.quickDemo.forEach((button) => { button.disabled = disabled; });
}

function render(data) {
  const active = Number(data.activeTime) || 0;
  const target = Number(data.targetTime) || 0;
  const remaining = Number.isFinite(Number(data.remainingTime)) ? Number(data.remainingTime) : Math.max(0, target - active);
  const percent = target ? Math.min(100, active / target * 100) : 0;
  elements.time.value = formatTime(remaining);
  elements.status.textContent = labelFor(data.state);
  elements.fill.style.width = `${percent}%`;
  elements.progress.setAttribute("aria-valuenow", String(Math.round(percent)));

  const uid = data.activeTagUid || "";
  if (uid !== displayedUid) {
    displayedUid = uid;
    elements.uid.value = uid;
    if (uid) elements.duration.value = Math.max(1, Math.round((Number(data.selectedDuration) || 1500) / 60));
  }

  const unavailable = requestPending || !connected || Boolean(data.sessionActive);
  elements.duration.disabled = unavailable || !uid;
  elements.save.disabled = unavailable || !uid || data.rfidReady === false;
  elements.quickDemo.forEach((button) => {
    button.disabled = unavailable || data.sensorsReady === false;
  });
  if (data.sensorsReady === false) elements.status.textContent = "Pencil sensors need setup";
  else if (data.rfidReady === false) elements.status.textContent = "RFID reader needs setup";
}

async function refresh() {
  try {
    const data = await api.getStatus();
    setConnection(true);
    render(data);
    if (elements.message.textContent === "Controller unavailable. Retrying…") elements.message.textContent = "";
  } catch {
    setConnection(false);
    elements.status.textContent = "Controller offline";
    elements.message.textContent = "Controller unavailable. Retrying…";
  }
}

elements.form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const minutes = Number(elements.duration.value);
  if (!elements.uid.value || !Number.isFinite(minutes) || minutes <= 0 || requestPending) return;
  requestPending = true;
  disableControls(true);
  elements.message.textContent = "Saving shell time…";
  try {
    render(await api.saveShell(elements.uid.value, Math.round(minutes * 60)));
    elements.message.textContent = `Saved ${minutes} minute${minutes === 1 ? "" : "s"} for shell ${elements.uid.value}.`;
  } catch (error) {
    elements.message.textContent = error.message;
  } finally {
    requestPending = false;
    await refresh();
  }
});

async function startDemo(duration) {
  if (requestPending) return;
  requestPending = true;
  disableControls(true);
  elements.message.textContent = `Demo armed for ${duration} seconds. Rest, then lift the pencil.`;
  try { render(await api.start(duration)); }
  catch (error) { elements.message.textContent = error.message; }
  finally { requestPending = false; await refresh(); }
}

elements.quickDemo.forEach((button) => {
  button.addEventListener("click", () => startDemo(Number(button.dataset.demoSeconds)));
});

elements.musicFile.addEventListener("change", () => {
  const [file] = elements.musicFile.files;
  if (!file) return;
  if (musicUrl) URL.revokeObjectURL(musicUrl);
  musicUrl = URL.createObjectURL(file);
  elements.audio.src = musicUrl;
  elements.audio.volume = Number(elements.volume.value);
  elements.musicToggle.disabled = false;
  elements.musicToggle.textContent = "Play";
  elements.trackName.textContent = file.name.replace(/\.[^.]+$/, "");
});

elements.musicToggle.addEventListener("click", async () => {
  if (elements.audio.paused) {
    try { await elements.audio.play(); }
    catch { elements.message.textContent = "Your browser could not play this audio file."; }
  } else elements.audio.pause();
});

elements.audio.addEventListener("play", () => { elements.musicToggle.textContent = "Pause"; });
elements.audio.addEventListener("pause", () => { elements.musicToggle.textContent = "Play"; });
elements.volume.addEventListener("input", () => { elements.audio.volume = Number(elements.volume.value); });
window.addEventListener("beforeunload", () => { if (musicUrl) URL.revokeObjectURL(musicUrl); });

refresh();
setInterval(refresh, config.statusPollMs);
