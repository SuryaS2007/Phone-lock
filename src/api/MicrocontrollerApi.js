export class MicrocontrollerApi {
  constructor(baseUrl) { this.baseUrl = baseUrl.replace(/\/$/, ""); }

  async getStatus() { return this._request("/status"); }

  async start(durationSeconds) {
    return this._request("/start", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ duration: durationSeconds })
    });
  }

  async saveShell(uid, durationSeconds) {
    return this._request("/shell", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ uid, duration: durationSeconds })
    });
  }

  async selectShell(uid) {
    return this._request("/shell/select", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ uid })
    });
  }

  async getShells() { return this._request("/shells"); }

  async reset() { return this._request("/reset", { method: "POST" }); }

  async getServoStatus() { return this._request("/servo/status"); }

  async moveServo(angle) {
    return this._request("/servo/move", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ angle })
    });
  }

  async runServoSequence(sequence) {
    return this._request("/servo/sequence", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(sequence)
    });
  }

  async detachServo() { return this._request("/servo/detach", { method: "POST" }); }

  async saveServoAngle(position, angle) {
    return this._request("/servo/save", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ position, angle })
    });
  }

  async _request(path, options) {
    let response;
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 5000);
    try {
      response = await fetch(`${this.baseUrl}${path}`, {
        ...options,
        cache: "no-store",
        signal: controller.signal
      });
    } catch (error) {
      const reason = error?.name === "AbortError" ? "request timed out" : "network request failed";
      throw new Error(`Cannot reach Focus Lock controller (${reason})`);
    } finally {
      clearTimeout(timeoutId);
    }
    if (!response.ok) throw new Error((await response.text()) || `Controller error (${response.status})`);
    return response.json();
  }
}
