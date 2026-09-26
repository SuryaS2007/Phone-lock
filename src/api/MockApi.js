/** Local stand-in for the eventual ESP32-facing API. */
export class MockApi {
  constructor(controller) { this.controller = controller; }
  async getStatus() { return this.controller.getStatus(); }
  async start({ duration }) { return this.controller.start(duration); }
  async reset() { this.controller.reset(); return this.controller.getStatus(); }
}
