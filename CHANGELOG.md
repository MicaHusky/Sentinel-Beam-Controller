# Changelog

## [Unreleased]
Nothing yet.

## [1.3.1] - current
### Changed
- The barrel strip is now described as simply 85 WS2812s (5 rings of 17). The
  old "160 physical / 85 active" split dated to an uncut bench strip that is
  long gone. `LED_TOTAL_COUNT` is removed and `LED_ACTIVE_COUNT` is renamed to
  `LED_COUNT`; the NeoPixel object is sized to 85 instead of 160. No behavior
  change - only the first 85 were ever addressed anyway. README pinout updated
  to match. (Bring-up-test sketches still carry the old 160 figure; they're
  frozen v1.0.0 artifacts recording what was tested on the bench.)

## [1.3.0]
A second momentary button cycles the LED mode.

### Added
- **Mode button on GPIO1.** Wired exactly like the trigger (INPUT_PULLUP,
  switch to GND, 20 ms debounce). Each press advances the LED mode one step and
  wraps around: `Normal` -> `Rainbow` -> `Normal` -> ...  New alt-modes slot
  into the same cycle later.
- **`ModeManager`** - owns the GPIO1 button and the current mode, and is the
  single source of truth for it. The debounce mirrors `TriggerManager`'s.
  Consistent with the independent-manager pattern: it never calls other
  managers; the main loop reads `mode()` each pass and, on a change, calls
  `applyMode()` to translate it into subsystem calls (today just
  `ledManager.setRainbowMode()`).
- Adding a future mode is three edits: a value in `LEDMode`, a case in
  `modeName()`, and a case in `applyMode()`.

### Changed
- The debug console's `ledMode=normal|rainbow` command now routes through
  `ModeManager` instead of poking `LEDManager` directly, so the console and the
  physical button can never disagree about the current mode. The `help`
  listing's "current values" line reports `ModeManager`'s mode.
- `SettingsController::begin()` and `DebugConsole::begin()` each take an extra
  `ModeManager*`.

## [1.2.0]
Boot sequence is now shown on the barrel: the five rings of 17 LEDs each act as
an independent loading bar for one boot phase.

### Added
- **Boot progress bar on the barrel.** Each ring fills in cyan (`33B5E5`, held
  at 50% brightness) as its phase completes:
  - ring 0: subsystem inits (LED -> trigger -> fog -> motor -> SD mount)
  - ring 1: `idle.wav` copied into PSRAM
  - ring 2: `fire.wav` copied into PSRAM
  - ring 3: `cooldown.wav` copied into PSRAM
  - ring 4: everything after (settings + debug console), with headroom left
    for future major subsystems (e.g. a web-UI init)
  Rings 1-3 sweep progressively as the bytes stream off the SD card. On a hard
  fail the bars freeze where they stalled, so the unfilled ring points at the
  subsystem that failed.
- **Boot result flash** on the whole barrel once the sequence finishes, per the
  project's long-standing plan: green x1 = all checks passed, red x2 = hard
  fail (firmware refuses to boot). Yellow x2 (recoverable / degraded) is wired
  through `BootResult::RECOVERABLE` but nothing triggers it yet - it needs the
  planned boot-time `config.txt` / audio-optional path.
- `LEDManager` boot API: `beginBootSequence()`, `setBootRingProgress()`,
  `bootFlash()`, `endBootSequence()`, plus a new `LEDState::BOOT`. The vent
  strips are held dark for the whole sequence and come up with the normal idle
  breathe once the board is ready.
- `AudioManager::setBootProgressCallback()` - a plain function-pointer hook the
  main sketch uses to drive the WAV-load rings. `AudioManager` still knows
  nothing about the LEDs; it only emits `(phase, fraction)` progress.

### Changed
- WAV files are now copied into PSRAM in `AUDIO_LOAD_CHUNK_BYTES` (16 KB) chunks
  instead of one blocking `f.read()`, so the boot progress rings can sweep as
  the data lands. The one-read-at-boot / SD-then-idle contract is unchanged.
- Boot order in `setup()`: `ledManager.begin()` now runs first (before trigger
  and fog) so the progress bar is live for the rest of boot.

### Notes
- The boot visuals (cyan/green/yellow/red colors, 50% brightness, flash timing)
  are boot-time-only constants in `Config.h` with no debug-console setter: the
  Serial console isn't up while they play, and there's no persistence layer to
  carry a change to the next boot.

## [1.1.0]
### Changed
- Default vent LED peak brightness (`LED_VENT_MAX_BRIGHTNESS`) lowered from
  50% to 25% - the vents were reading brighter than intended next to the
  barrel at idle. Still runtime-tunable via the Serial debug console.

## [1.0.0]
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
