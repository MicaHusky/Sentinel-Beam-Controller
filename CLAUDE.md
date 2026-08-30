# Sentinel Beam Firmware - Context for Claude

This file is read automatically at the start of every Claude Code session in
this repo. `README.md` covers hardware/pinout/setup and `CHANGELOG.md` covers
what was built and when. This file covers the *why* behind decisions that
aren't obvious from the code alone - things worth knowing before changing
them.

## Project

A full-scale, functional Halo Infinite Sentinel Beam prop. ESP32-S3 firmware
coordinating a trigger, PSRAM-cached audio, a stepper-driven rotating barrel,
three WS2812 LED zones, and a fogger. Originally built with ChatGPT; rebuilt
from scratch with Claude (claude.ai) because the ChatGPT-authored code
frequently didn't work as given.

## Hard constraints - do not change without discussion

- **GPIO assignments** (see README's pinout table) - many were chosen
  specifically to avoid the OPI PSRAM range, USB D-/D+, and strapping pins.
- **SD card is read exactly once, at boot, before anything else runs.**
  All three WAV files get copied into PSRAM, then the SD card is never
  touched again for the rest of runtime. This is deliberate - not an
  oversight to "optimize away."
- **The stepper's EN pin auto-disables at rest.** This is a physical safety
  requirement, not a power-saving nicety (though it's also that): the
  barrel is a foot-long, ~100mm diameter exposed 3D-printed rotating part.
  If a finger gets caught, the coils must not be energized holding the
  barrel in motion - it needs to be freely stoppable by hand. Don't change
  this to "always-enabled" for smoothness without discussing the tradeoff.
- **Trigger release currently does a full controlled deceleration** (~0.5s
  driven ramp-down, EN disables only once fully stopped) rather than an
  instant free-coast. This was a deliberate choice over the more
  safety-conservative "cut power immediately" option - the tradeoff was
  discussed and accepted, not overlooked.

## Design philosophy

- Each subsystem (`TriggerManager`, `AudioManager`, `MotorManager`,
  `LEDManager`, `FogManager`) is independent and only reacts to explicit
  calls from the main sketch's trigger-driven state machine
  (`IDLE`/`FIRING`/`COOLDOWN`). Managers don't call each other directly.
  Keep new managers consistent with this pattern unless there's a specific
  reason not to (see the debug console's `SettingsController` for the one
  exception - shared validation logic used by multiple front-ends).
- Everything that's remotely a matter of taste (colors, timing, brightness,
  gain) should be runtime-tunable via the Serial debug console
  (`SettingsController`/`DebugConsole`), not just a hardcoded constant.
  `Config.h` values are boot-time defaults, not the only way to set them.
- Prefer asking before making a call on ambiguous creative/behavioral
  decisions (exact colors, timing values, what happens in an edge case)
  rather than silently picking one and moving on - past sessions have gone
  back and forth on these several times (e.g. cooldown fade vs. hard-switch,
  idle/fire color assignment) and getting it wrong costs a re-do.
- Serial logging is expected on every meaningful state transition and every
  boot-time pass/fail check. This has been genuinely useful for hardware
  bring-up - don't strip it out for cleanliness.

## Notable decisions worth knowing before touching related code

- **Cooldown LED reversion is keyed to 5 manually-tuned "ka-chunk" audio
  timestamps** (`kachunk1`-`kachunk5` + `kachunkTail`), not a fixed duration.
  Only as many kachunks as LED rings actually lit up are used, so releasing
  the trigger early naturally plays a shorter portion of `cooldown.wav` and
  finishes sooner. This replaced an earlier fixed-duration fade design -
  don't revert to that without discussing why.
- **The barrel's idle color is the orangish one (`DF5709`), fire color is
  purplish (`6470E2`)** - this was deliberately swapped from an earlier,
  more intuitive-seeming assignment. Don't "fix" this back.
- **The two vent LED strips never change hue** - only brightness - between
  idle and firing. This was explicit and specific; don't add a color wipe
  to them to match the barrel's behavior.
- **Boot progress bar (v1.2.0):** during `setup()` the 5 barrel rings each act
  as an independent 17-LED loading bar for one boot phase - ring 0 = subsystem
  inits (LED/trigger/fog/motor/SD mount), rings 1-3 = the `idle`/`fire`/
  `cooldown` WAV loads (which sweep as bytes stream in - this is why
  `loadAssetToPSRAM` reads in chunks now), ring 4 = everything after, with
  deliberate headroom for future major subsystems. Fills cyan at 50%. On a
  hard fail the bars freeze where they stalled (diagnostic), then the barrel
  flashes the result code: green x1 = OK, red x2 = hard fail. Yellow x2
  (`BootResult::RECOVERABLE`) is wired but unreachable until the `config.txt`
  / audio-optional path below exists. `ledManager.begin()` runs first in
  `setup()` now so the bar is live for the rest of boot. The boot visuals are
  boot-time-only `Config.h` constants with no console setter, on purpose (the
  console isn't up yet and there's no persistence).
- **The GPIO1 mode button cycles LED modes** (`Normal` -> `Rainbow` -> ...,
  wrapping). `ModeManager` owns the button and is the *single source of truth*
  for the current mode - the debug console's `ledMode=` command routes through
  it too, so the two can't disagree. `ModeManager` follows the
  independent-manager rule (it never calls `LEDManager`); the main loop polls
  `mode()` and calls `applyMode()` in `SentinelBeam.ino` to act on a change.
  Adding a mode = a value in `LEDMode`, a `modeName()` case, an `applyMode()`
  case. The button debounce is a deliberate copy of `TriggerManager`'s rather
  than a shared helper, to keep the hardware-validated trigger path untouched.
- A WiFi captive-portal control page was built, then explicitly removed
  (reverted to WiFi-less) because there was no persistence layer for its
  changes yet. `SettingsController` was kept because the Serial console
  still uses it. If WiFi control comes back, it's meant to write to the
  config-file system below, not just be another runtime-only tuner.

## Backlog / known future work (not yet built)

- **Boot-time `config.txt` on the SD card**, read once before the WAV files
  load, then the card goes idle for the rest of runtime. Meant to hold:
  which WAV plays in which state, barrel speed/direction, audio mode
  (normal/quiet/silent), fog on/off, and independent enable/disable for
  every subsystem (so e.g. a jammed barrel can be disabled at a convention
  while keeping lights/sound/fog working). A missing config file is a
  recoverable case, not a hard fail. LED settings are explicitly deferred
  out of this config file until later. The barrel-LED boot status feedback
  this was meant to pair with is now built (see "Boot progress bar" above):
  green x1 / red x2 are live; **yellow x2 = recoverable (fall back to
  lights+motion only) becomes reachable once this config path exists** and
  the firmware has a real degraded-run mode to enter.
- **Additional LED banks**: a "radiator" bank and body-detail LEDs, plus one
  spare output for future expansion. Should follow the same state machine
  as the vent strips/barrel, colors/behavior not yet decided.
- **Alt in-game-accurate cooldown**: barrel rotating ~20-something degrees
  in reverse per kachunk beat (roughly double the normal spin accel),
  synchronized with the audio and LED beats. Scoped as feasible, never
  approved to build.
- Foggers are a physically removable module (for convention rules) - no
  hardware presence-detect, purely software config-driven (see config.txt
  above).
- Vibration motor and servo(s) were mentioned early on as future modules;
  never revisited since.

## Hardware bring-up

`bring-up-tests/` holds minimal standalone sketches (independent of the main
firmware) used to validate each subsystem on a freshly populated PCB one at
a time. All of them have passed as of `v1.0.0`. If you're debugging a
hardware issue on a *new* board revision, these are the right starting
point rather than debugging through the full firmware.
