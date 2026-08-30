# Changelog

## [Unreleased]
Nothing yet.

## [1.0.0]
First fully hardware-tested build - every subsystem on the PCB has passed its
own bring-up test (see `bring-up-tests/`).

### Added
- Trigger-driven state machine: idle -> firing -> cooldown -> idle, with
  software-debounced edge detection and support for re-firing mid-cooldown.
- Audio: `idle.wav` / `fire.wav` / `cooldown.wav` copied into PSRAM once at
  boot; SD card is never touched again afterward.
- Motor: FastAccelStepper-driven barrel spin-up on fire, controlled
  deceleration on release, auto-disabled EN coils at rest (finger-safety +
  power saving).
- Barrel LED strip (85 of 160 WS2812s, 5 sets of 17):
  - Idle: breathing pulse between a min and max brightness.
  - Firing: sets sequentially fade idle-color -> fire-color.
  - Cooldown: sets hard-switch back to idle color one by one, in reverse
    order, cued by 5 manually-tuned "ka-chunk" audio timestamps + a tail
    time - only as many beats as rings actually lit are used, so an early
    release finishes the sequence sooner.
  - Rainbow mode: full-strip color-wheel chase overriding normal rendering.
- Two vent LED strips (GPIO40/41, 80 LEDs each): single fixed color, breathes
  in sync with the barrel at idle (own lower peak brightness), jumps to full
  brightness while firing/cooling down without changing hue, also follows
  rainbow mode.
- Fogger: MOSFET on GPIO9, on for the duration of firing only.
- Serial debug console (`help` for the list): live-tune colors, wipe/breathe
  timing, brightness ranges, kachunk timestamps/tail, audio gain, and
  rainbow mode/speed. Runtime-only, resets to `Config.h` defaults on reboot.
- `bring-up-tests/`: a series of minimal standalone sketches for testing SD,
  PSRAM, the barrel LED strip, audio playback, the stepper, the trigger, and
  the fogger independently while populating a fresh PCB.

### Removed
- WiFi captive-portal control page - reverted to WiFi-less operation. May
  return later once there's a config-file system to persist its changes to.
