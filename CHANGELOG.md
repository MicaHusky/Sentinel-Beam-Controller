# Changelog

## [Unreleased]
Nothing yet.

## [1.0.0] - current
First fully hardware-tested build - every subsystem on the PCB has passed its
own bring-up test (see `bring-up-tests/`). WiFi control page removed (see
0.9.0) in favor of staying WiFi-less for now; two new vent LED strips added.

### Added
- Two vent LED strips (GPIO40/GPIO41, 80 WS2812s each): single fixed color
  that never changes hue, only intensity - breathes in sync with the barrel
  at idle (shared floor, own lower peak brightness), jumps to full brightness
  while firing/cooling down, and follows rainbow mode alongside the barrel.
- GitHub repo, README, this changelog, and a `FIRMWARE_VERSION` constant
  that prints over Serial at boot.

### Removed
- The WiFi captive-portal control page introduced in 0.9.0 - reverted to
  WiFi-less operation. `SettingsController` (the shared logic behind it) was
  kept, since the Serial debug console still uses it.

## [0.9.0]
### Added
- Phone-based WiFi control page: ESP32 hosts a captive-portal access point
  ("Sentinel Beam Control"), serving a page with color pickers, numeric
  inputs for every timing/brightness setting, a rainbow mode checkbox, a
  live-updating speed slider, and a press-and-hold virtual trigger button.
- `SettingsController`: extracted the debug console's key/value validation
  logic into its own shared class so both the Serial console and the web
  page apply settings through identical, single-sourced validation.
- `TriggerManager::setWebOverride()`: lets the web page's fire button act as
  a second physical trigger, OR'd with the real switch before debouncing.
### Fixed
- Mobile touch-drag near sliders was triggering text-selection instead of
  moving the slider; fixed with `user-select`/`touch-action` CSS.
- The fixed Apply/Fire buttons could cover input fields (often the rainbow
  speed slider) once the on-screen keyboard was up; fixed by hiding those
  buttons while any text field has focus.

## [0.8.0]
### Added
- `bring-up-tests/`: a series of minimal, standalone sketches (independent of
  the main firmware) for testing one PCB subsystem at a time while
  populating a fresh board - SD/PSRAM, barrel LED mirroring, audio playback,
  stepper motor, trigger-gated start, and the fogger MOSFET, each building
  on the last.

## [0.7.0]
### Added
- Keyframe-driven cooldown: 5 manually-tunable "ka-chunk" timestamps (from
  the start of `cooldown.wav`) plus one tail time replace the old fixed
  `cooldownWipeSeconds` duration. Ring N reverts the instant kachunk N's
  timestamp is crossed; only as many kachunks as rings actually lit are
  used, so releasing the trigger early naturally plays a shorter portion of
  the cooldown audio and finishes sooner.
- `kachunk1`-`kachunk5`, `kachunkTimestamps` (all 5 at once), and
  `kachunkTail` debug console commands, each validated to stay ascending.

## [0.6.0]
### Changed
- Cooldown ring reversion switched from a gradual fade back to a HARD
  SWITCH (instant cut, no gradient) - matches the in-game weapon's snappier
  feel better than a fade did.
- Retuned default settings: idle color is now the orangish/peach `DF5709`,
  fire color the purplish `6470E2` (previously the reverse), plus new
  defaults for wipe/breathe timing and brightness range.

## [0.5.0]
### Added
- Serial debug console (`DebugConsole`): `key=value` commands to live-tune
  LED colors, wipe/breathe timing, brightness range, and audio gain without
  reflashing; `help` lists everything tunable with current values.
- Rainbow mode: `ledMode=rainbow` overrides normal LED rendering with a
  full-strip color-wheel chase; `rainbowSpeed=` tunes how fast it moves.

## [0.4.0]
### Added
- `FogManager`: MOSFET on GPIO9, straight digital on/off, active only for
  the duration of the `FIRING` state.

## [0.3.0]
### Added
- `LEDManager`: barrel WS2812 strip (85 of 160 LEDs, 5 sets of 17).
  - Idle: breathing pulse between a min and max brightness.
  - Firing: sets sequentially fade idle-color -> fire-color, one at a time.
  - Cooldown (initial version): sets fade back to idle color in reverse
    order over a fixed duration, then brightness eases back down to idle
    level.

## [0.2.0]
### Added
- Full three-track audio: `idle.wav` loops continuously except while firing;
  `fire.wav` plays on trigger press; `cooldown.wav` plays after release or
  natural EOF, then idle resumes looping.
- Complete `IDLE` / `FIRING` / `COOLDOWN` state machine, refire-during-
  cooldown support, and full boot-time PSRAM loading for all three files.
### Fixed
- A trigger release occurring after `fire.wav` had already reached natural
  EOF (state already in `COOLDOWN`) was incorrectly re-triggering the whole
  cooldown sequence a second time. Now gated on actually being in `FIRING`.

## [0.1.0]
### Added
- Initial from-scratch rewrite of the firmware (replacing prior
  ChatGPT-authored code): `TriggerManager` (debounced edge detection),
  `MotorManager` (FastAccelStepper spin-up/spin-down with EN
  auto-disable for finger-safety and power saving), and `AudioManager`
  (`fire.wav` cached into PSRAM at boot, SD card never touched again
  afterward). Basic press-to-fire, release-to-stop behavior.
