import { MicrocontrollerApi } from "./api/MicrocontrollerApi.js";
import { config } from "./config.js";

const api = new MicrocontrollerApi(config.controllerUrl || window.location.origin);
const elements = {
  connection: document.querySelector("#connection"), time: document.querySelector("#time"),
  status: document.querySelector("#status"), progress: document.querySelector(".progress"),
  fill: document.querySelector("#progress-fill"), form: document.querySelector("#start-form"),
  duration: document.querySelector("#duration"), start: document.querySelector("#start"),
  message: document.querySelector("#message"), musicFile: document.querySelector("#music-file"),
  musicToggle: document.querySelector("#music-toggle"), volume: document.querySelector("#volume"),
  audio: document.querySelector("#audio"), trackName: document.querySelector("#track-name")
};
let connected = false;
let requestPending = false;
let musicUrl = null;

function formatTime(seconds) {
  const value = Math.max(0, Math.floor(Number(seconds) || 0));
  return `${String(Math.floor(value / 60)).padStart(2, "0")}:${String(value % 60).padStart(2, "0")}`;
}

function labelFor(state) {
  return ({ IDLE:"Insert phone", READY:"Ready", LOCKED_PAUSED:"Paused", LOCKED_STUDYING:"Studying", COMPLETE:"Complete — phone unlocked" })[state] || "Ready";
}

function setConnection(value) {
  connected = value;
  elements.connection.classList.toggle("offline", !value);
  elements.connection.classList.toggle("online", value);
  elements.connection.lastChild.textContent = value ? " Connected" : " Offline";
  if (!value) elements.start.disabled = true;
}

function render(data) {
  const active = Number(data.activeTime) || 0;
  const target = Number(data.targetTime) || 0;
  const percent = target ? Math.min(100, active / target * 100) : 0;
  const remaining = target ? Math.max(0, target - active) : 0;
  elements.time.value = formatTime(remaining);
  elements.status.textContent = labelFor(data.state);
  elements.fill.style.width = `${percent}%`;
  elements.progress.setAttribute("aria-valuenow", String(Math.round(percent)));
  elements.duration.disabled = Boolean(data.sessionActive);
  elements.start.disabled = requestPending || !connected || Boolean(data.sessionActive) || data.phonePresent === false;
  elements.start.textContent = data.sessionActive ? "In progress" : "Start";
}

async function refresh() {
  try {
    const data = await api.getStatus();
    setConnection(true); render(data);
    if (elements.message.textContent === "Controller unavailable. Retrying…") elements.message.textContent = "";
  } catch {
    setConnection(false);
    elements.status.textContent = "Controller offline";
    elements.message.textContent = "Controller unavailable. Retrying…";
  }
}

elements.form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const duration = Number(elements.duration.value) * 60;
  if (!Number.isFinite(duration) || duration <= 0) return;
  requestPending = true; elements.start.disabled = true; elements.message.textContent = "Starting session…";
  try {
    render(await api.start(duration));
    elements.message.textContent = "Session started. Phone locked.";
  } catch (error) { elements.message.textContent = error.message; }
  finally { requestPending = false; await refresh(); }
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
    try { await elements.audio.play(); } catch { elements.message.textContent = "Your browser could not play this audio file."; }
  } else elements.audio.pause();
});

elements.audio.addEventListener("play", () => { elements.musicToggle.textContent = "Pause"; });
elements.audio.addEventListener("pause", () => { elements.musicToggle.textContent = "Play"; });
elements.volume.addEventListener("input", () => { elements.audio.volume = Number(elements.volume.value); });
window.addEventListener("beforeunload", () => { if (musicUrl) URL.revokeObjectURL(musicUrl); });

refresh();
setInterval(refresh, config.statusPollMs);
