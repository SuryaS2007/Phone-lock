export class MicrocontrollerApi {
  constructor(baseUrl) { this.baseUrl = baseUrl.replace(/\/$/, ""); }

  async getStatus() { return this.#request("/status"); }

  async start(durationSeconds) {
    return this.#request("/start", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ duration: durationSeconds })
    });
  }

  async saveShell(uid, durationSeconds) {
    return this.#request("/shell", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ uid, duration: durationSeconds })
    });
  }

  async getShells() { return this.#request("/shells"); }

  async reset() { return this.#request("/reset", { method: "POST" }); }

  async #request(path, options) {
    let response;
    try {
      response = await fetch(`${this.baseUrl}${path}`, { ...options, signal: AbortSignal.timeout(3000) });
    } catch {
      throw new Error("Cannot reach Focus Lock controller");
    }
    if (!response.ok) throw new Error((await response.text()) || `Controller error (${response.status})`);
    return response.json();
  }
}
