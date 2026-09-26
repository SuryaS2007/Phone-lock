import { HardwareAdapter } from "./HardwareAdapter.js";

export class SimulatedHardware extends HardwareAdapter {
  #phonePresent = false;
  #pencilPresent = true;
  #locked = false;
  #listeners = new Set();

  isPhonePresent() { return this.#phonePresent; }
  isPencilPresent() { return this.#pencilPresent; }
  isLocked() { return this.#locked; }

  lockPhone() {
    if (!this.#phonePresent) throw new Error("Cannot lock: no phone is detected.");
    this.#locked = true;
    this.#emit();
  }

  unlockPhone() {
    this.#locked = false;
    this.#emit();
  }

  setPhonePresent(value) {
    this.#phonePresent = Boolean(value);
    this.#emit();
  }

  setPencilPresent(value) {
    this.#pencilPresent = Boolean(value);
    this.#emit();
  }

  subscribe(listener) {
    this.#listeners.add(listener);
    return () => this.#listeners.delete(listener);
  }

  #emit() {
    const snapshot = {
      phonePresent: this.#phonePresent,
      pencilPresent: this.#pencilPresent,
      locked: this.#locked
    };
    this.#listeners.forEach((listener) => listener(snapshot));
  }
}
