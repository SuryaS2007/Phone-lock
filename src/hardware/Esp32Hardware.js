import { HardwareAdapter } from "./HardwareAdapter.js";

/**
 * Prepared adapter for a future ESP32 HTTP server. It is intentionally not
 * instantiated yet. Match the ESP32 firmware routes to these methods, then
 * select this adapter in src/config.js.
 */
export class Esp32Hardware extends HardwareAdapter {
  constructor(baseUrl) {
    super();
    this.baseUrl = baseUrl.replace(/\/$/, "");
    this.status = { phonePresent: false, pencilPresent: true, locked: false };
    this.listeners = new Set();
  }

  isPhonePresent() { return this.status.phonePresent; }
  isPencilPresent() { return this.status.pencilPresent; }
  isLocked() { return this.status.locked; }

  async refresh() {
    const response = await fetch(`${this.baseUrl}/status`);
    if (!response.ok) throw new Error(`ESP32 status failed (${response.status})`);
    this.status = { ...this.status, ...(await response.json()) };
    this.listeners.forEach((listener) => listener(this.status));
  }

  async lockPhone() { await this.#post("/lock"); await this.refresh(); }
  async unlockPhone() { await this.#post("/unlock"); await this.refresh(); }
  subscribe(listener) { this.listeners.add(listener); return () => this.listeners.delete(listener); }

  async #post(path) {
    const response = await fetch(`${this.baseUrl}${path}`, { method: "POST" });
    if (!response.ok) throw new Error(`ESP32 command failed (${response.status})`);
  }
}
