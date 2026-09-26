import test from "node:test";
import assert from "node:assert/strict";
import { SessionController, SessionState } from "../src/core/SessionController.js";
import { SimulatedHardware } from "../src/hardware/SimulatedHardware.js";

function setup() {
  let time = 0;
  const hardware = new SimulatedHardware();
  const controller = new SessionController(hardware, { now: () => time });
  return { hardware, controller, advance: (ms) => { time += ms; controller.tick(); } };
}

test("moves from IDLE to READY when a phone is inserted", () => {
  const { hardware, controller } = setup();
  assert.equal(controller.getState(), SessionState.IDLE);
  hardware.setPhonePresent(true);
  assert.equal(controller.getState(), SessionState.READY);
});

test("starts locked and paused while the pencil is down", () => {
  const { hardware, controller } = setup();
  hardware.setPhonePresent(true);
  controller.start(30);
  assert.equal(controller.getState(), SessionState.LOCKED_PAUSED);
  assert.equal(hardware.isLocked(), true);
});

test("accumulates only active time across pencil pauses", () => {
  const { hardware, controller, advance } = setup();
  hardware.setPhonePresent(true);
  controller.start(30);
  hardware.setPencilPresent(false);
  advance(10_000);
  hardware.setPencilPresent(true);
  advance(5_000);
  assert.equal(controller.getStatus().activeTime, 10);
  hardware.setPencilPresent(false);
  advance(20_000);
  assert.equal(controller.getState(), SessionState.COMPLETE);
  assert.equal(controller.getStatus().activeTime, 30);
  assert.equal(hardware.isLocked(), false);
});

test("rejects a session without a phone", () => {
  const { controller } = setup();
  assert.throws(() => controller.start(10), /Insert your phone/);
});

test("removing the phone resets an active simulated session", () => {
  const { hardware, controller } = setup();
  hardware.setPhonePresent(true);
  controller.start(10);
  hardware.setPhonePresent(false);
  assert.equal(controller.getState(), SessionState.IDLE);
  assert.equal(hardware.isLocked(), false);
  assert.equal(controller.getStatus().targetTime, 0);
});
