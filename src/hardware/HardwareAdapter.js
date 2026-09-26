/**
 * Contract shared by simulated and real hardware adapters.
 * Implementations expose sensor readings, servo commands, and change events.
 */
export class HardwareAdapter {
  isPhonePresent() { throw new Error("isPhonePresent() must be implemented"); }
  isPencilPresent() { throw new Error("isPencilPresent() must be implemented"); }
  isLocked() { throw new Error("isLocked() must be implemented"); }
  lockPhone() { throw new Error("lockPhone() must be implemented"); }
  unlockPhone() { throw new Error("unlockPhone() must be implemented"); }
  subscribe() { throw new Error("subscribe() must be implemented"); }
}
