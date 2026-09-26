export const SessionState = Object.freeze({
  IDLE: "IDLE",
  READY: "READY",
  LOCKED_PAUSED: "LOCKED_PAUSED",
  LOCKED_STUDYING: "LOCKED_STUDYING",
  COMPLETE: "COMPLETE"
});

export class SessionController {
  #hardware;
  #now;
  #targetMs = 0;
  #accumulatedMs = 0;
  #runningSince = null;
  #sessionActive = false;
  #complete = false;
  #listeners = new Set();

  constructor(hardware, { now = () => performance.now() } = {}) {
    this.#hardware = hardware;
    this.#now = now;
    hardware.subscribe(() => this.#onHardwareChange());
  }

  start(durationSeconds) {
    const seconds = Number(durationSeconds);
    if (!this.#hardware.isPhonePresent()) throw new Error("Insert your phone before starting.");
    if (!Number.isFinite(seconds) || seconds <= 0) throw new Error("Choose a valid study duration.");

    this.#targetMs = seconds * 1000;
    this.#accumulatedMs = 0;
    this.#runningSince = null;
    this.#complete = false;
    this.#sessionActive = true;
    this.#hardware.lockPhone();
    if (!this.#hardware.isPencilPresent()) this.#runningSince = this.#now();
    this.#emit();
    return this.getStatus();
  }

  reset() {
    this.#sessionActive = false;
    this.#complete = false;
    this.#targetMs = 0;
    this.#accumulatedMs = 0;
    this.#runningSince = null;
    this.#hardware.unlockPhone();
    this.#emit();
  }

  tick() {
    if (this.#sessionActive && this.getActiveMs() >= this.#targetMs) this.#finish();
    this.#emit();
    return this.getStatus();
  }

  getActiveMs() {
    const liveMs = this.#runningSince === null ? 0 : Math.max(0, this.#now() - this.#runningSince);
    return Math.min(this.#targetMs || Infinity, this.#accumulatedMs + liveMs);
  }

  getState() {
    if (this.#complete) return SessionState.COMPLETE;
    if (!this.#hardware.isPhonePresent()) return SessionState.IDLE;
    if (!this.#sessionActive) return SessionState.READY;
    return this.#runningSince === null ? SessionState.LOCKED_PAUSED : SessionState.LOCKED_STUDYING;
  }

  getStatus() {
    const activeMs = this.getActiveMs();
    return {
      phonePresent: this.#hardware.isPhonePresent(),
      pencilPresent: this.#hardware.isPencilPresent(),
      locked: this.#hardware.isLocked(),
      sessionActive: this.#sessionActive,
      activeTime: Math.floor(activeMs / 1000),
      activeTimeMs: activeMs,
      targetTime: Math.ceil(this.#targetMs / 1000),
      targetTimeMs: this.#targetMs,
      remainingTimeMs: Math.max(0, this.#targetMs - activeMs),
      state: this.getState()
    };
  }

  subscribe(listener) {
    this.#listeners.add(listener);
    listener(this.getStatus());
    return () => this.#listeners.delete(listener);
  }

  #onHardwareChange() {
    if (this.#sessionActive && !this.#hardware.isPhonePresent()) {
      this.reset();
      return;
    }

    if (this.#sessionActive) {
      const pencilDown = this.#hardware.isPencilPresent();
      if (pencilDown && this.#runningSince !== null) {
        this.#accumulatedMs += Math.max(0, this.#now() - this.#runningSince);
        this.#runningSince = null;
      } else if (!pencilDown && this.#runningSince === null) {
        this.#runningSince = this.#now();
      }
    }
    this.#emit();
  }

  #finish() {
    this.#accumulatedMs = this.#targetMs;
    this.#runningSince = null;
    this.#sessionActive = false;
    this.#complete = true;
    this.#hardware.unlockPhone();
  }

  #emit() {
    const status = this.getStatus();
    this.#listeners.forEach((listener) => listener(status));
  }
}
