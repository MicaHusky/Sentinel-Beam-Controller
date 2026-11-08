#pragma once
#include <Arduino.h>

// =============================================================================
// Sentinel Beam - Config.h
// Central place for pinout + tunable constants. See project docs for the
// "DO NOT CHANGE WITHOUT DISCUSSION" list before touching values in here.
// =============================================================================

// Bump this alongside CHANGELOG.md whenever a build is worth calling a
// version - printed over Serial at boot so a flashed board always tells you
// exactly what firmware it's running.
constexpr const char* FIRMWARE_VERSION = "1.5.0";

// ---------- Trigger ----------
constexpr uint8_t  PIN_TRIGGER          = 4;   // INPUT_PULLUP, switch pulls to GND
constexpr uint16_t TRIGGER_DEBOUNCE_MS  = 20;

// ---------- Mode button ----------
// Second momentary button, wired exactly like the trigger (INPUT_PULLUP,
// switch shorts the pin to GND, so LOW = pressed). GPIO1 is clear of the
// strapping pins (0/3/45/46), USB D-/D+ (19/20), and the OPI PSRAM range.
// Gestures (see ModeManager):
//   single / double / triple press -> load /AltMode1 / /AltMode2 / /AltMode3
//   5 presses                      -> display mode (/DisplayMode)
//   long press (held)              -> revert to stock, as if freshly booted
// While in display mode every gesture is ignored EXCEPT a very long hold
// (DISPLAY_EXIT_HOLD_MS) back to stock, so bumping the prop while picking it
// up doesn't knock it out of display mode.
// A press made while firing or cooling down is queued and applied the moment
// the beam is back at IDLE. Both timings below are boot defaults - the debug
// console's longPressMs= / multiPressGapMs= tune them live.
constexpr uint8_t  PIN_MODE_BUTTON         = 1;
constexpr uint16_t MODE_BUTTON_DEBOUNCE_MS = 20;
constexpr uint16_t MODE_LONG_PRESS_MS      = 1000; // held this long -> stock (fires while still held)
constexpr uint16_t MODE_MULTI_PRESS_GAP_MS = 350;  // max gap between taps of one double/triple press
constexpr uint16_t DISPLAY_EXIT_HOLD_MS    = 5000; // hold this long to leave display mode (-> stock)

// ---------- Alt modes (per-mode sound + settings folders on the SD card) ----------
// Each alt mode is a folder at the card root holding its own idle.wav /
// fire.wav / cooldown.wav plus a human-readable MODE_CONFIG_FILENAME of
// key=value lines (same keys as the debug console). Stock = the WAVs at the
// card root + the Config.h defaults below. Any of an alt mode's files may be
// left out: a missing WAV borrows the stock one from the card root (status
// indicator shows turquoise instead of green), a missing mode.txt means stock
// settings. If ALL FOUR are missing, or mode.txt has a bad line, the mode is
// rejected: the status LEDs flash red and the prop reverts to stock.
constexpr const char* ALT_MODE_DIR_PREFIX   = "/AltMode";  // + "1".."3"
constexpr const char* MODE_CONFIG_FILENAME  = "mode.txt";
constexpr size_t      MODE_CONFIG_MAX_BYTES = 8192;        // sanity cap on mode.txt size

// ---------- microSD (read at boot + on an explicit mode switch, idle otherwise) ----------
constexpr uint8_t  PIN_SD_CS   = 10;
constexpr uint8_t  PIN_SD_SCK  = 5;
constexpr uint8_t  PIN_SD_MISO = 6;
constexpr uint8_t  PIN_SD_MOSI = 7;
constexpr const char* IDLE_WAV_PATH     = "/idle.wav";
constexpr const char* FIRE_WAV_PATH     = "/fire.wav";
constexpr const char* COOLDOWN_WAV_PATH = "/cooldown.wav";

// ---------- Audio (PCM5102A I2S DAC) ----------
constexpr uint8_t PIN_I2S_DOUT = 11; // -> DIN on PCM5102A
constexpr uint8_t PIN_I2S_BCLK = 12;
constexpr uint8_t PIN_I2S_LRCK = 13;
constexpr float    AUDIO_GAIN  = 0.15f;

// Each WAV is copied off the SD card into PSRAM in chunks this size, rather
// than one blocking read, so the boot progress bar (see below) can sweep as
// the bytes land. Purely a cosmetic granularity knob - larger means fewer LED
// updates during the load, smaller means a smoother sweep.
constexpr size_t AUDIO_LOAD_CHUNK_BYTES = 16384;

// ---------- Stepper (TMC2209, STEP/DIR mode) ----------
constexpr uint8_t  PIN_STEPPER_STEP   = 15;
constexpr uint8_t  PIN_STEPPER_DIR    = 16;
constexpr uint8_t  PIN_STEPPER_ENABLE = 17;  // active LOW

constexpr uint32_t MOTOR_SPEED_HZ     = 7467;   // ~70 RPM at barrel w/ 4:1 reduction
constexpr uint32_t MOTOR_ACCEL_HZ_S2  = 14934;  // ~0.5s spin-up / spin-down
// Steps per barrel revolution, implied by MOTOR_SPEED_HZ ~= 70 RPM
// (7467 * 60 / 70 = 6400). Used to turn display-mode twitchRPM into Hz.
constexpr uint32_t MOTOR_STEPS_PER_BARREL_REV = 6400;

// ---------- LEDs (WS2812 addressable strip) ----------
constexpr uint8_t  PIN_LED_DATA     = 18; // clear of SD/I2S/stepper, USB D-/D+ (19/20), and OPI PSRAM pins (26-32)

constexpr uint16_t LED_COUNT        = 85; // barrel strip: 5 rings of 17 WS2812s
constexpr uint8_t  LED_SET_SIZE     = 17;
constexpr uint8_t  LED_NUM_SETS     = LED_COUNT / LED_SET_SIZE; // 5

constexpr float    LED_FIRE_WIPE_SECONDS     = 6.0f;  // idle-color->fire-color wipe duration while firing
constexpr uint32_t LED_BRIGHTNESS_FADE_MS    = 800;   // 100% -> idle level, once fully back to idle color
constexpr uint32_t LED_IDLE_BREATHE_PERIOD_MS = 3000; // one full idle breathe cycle

// Cooldown ring-reversion is keyed to these 5 manually-tuned kachunk timestamps
// (seconds from the start of cooldown.wav) rather than a fixed duration -
// ring N reverts the instant kachunk N's timestamp is crossed. Tail is extra
// time held after the LAST NEEDED kachunk before the cooldown sequence ends
// (covers that hit's natural ring-out/decay).
constexpr float LED_KACHUNK_1_SEC    = 0.260f;
constexpr float LED_KACHUNK_2_SEC    = 0.680f;
constexpr float LED_KACHUNK_3_SEC    = 1.080f;
constexpr float LED_KACHUNK_4_SEC    = 1.490f;
constexpr float LED_KACHUNK_5_SEC    = 1.964f;
constexpr float LED_KACHUNK_TAIL_SEC = 0.300f;

constexpr float LED_IDLE_MIN_BRIGHTNESS = 0.05f;
constexpr float LED_IDLE_MAX_BRIGHTNESS = 0.15f;
constexpr float LED_FIRE_BRIGHTNESS      = 1.0f; // barrel while firing + kachunk wipe (fireBrightness=)
constexpr float LED_VENT_FIRE_BRIGHTNESS = 0.30f; // vents while firing + kachunk wipe (ventFireBrightness=)

constexpr uint8_t LED_COLOR_IDLE_DEFAULT_R = 0xDF, LED_COLOR_IDLE_DEFAULT_G = 0x57, LED_COLOR_IDLE_DEFAULT_B = 0x09;
constexpr uint8_t LED_COLOR_FIRE_DEFAULT_R = 0x64, LED_COLOR_FIRE_DEFAULT_G = 0x70, LED_COLOR_FIRE_DEFAULT_B = 0xE2;

// Any color setting (idleColor / fireColor / ventColor / ventFireColor) can be
// the word "rainbow" instead of a hex code: that slot then renders the
// color-wheel chase. Rainbow never breathes - it holds rainbowBrightness in
// every state. A rainbow fireColor switches the whole barrel at once on the
// trigger pull rather than doing the ring-by-ring wipe.
constexpr float LED_RAINBOW_SPEED_DEFAULT      = 40.0f; // color-wheel positions/sec the chase advances by
constexpr float LED_RAINBOW_BRIGHTNESS_DEFAULT = 1.0f;  // fixed brightness of any rainbow slot, 0..1

// ---------- Boot progress bar (barrel rings as a 5-phase loading indicator) ----------
// During setup() the 5 barrel rings each act as an INDEPENDENT 17-LED loading
// bar, one per boot phase:
//   ring 0 : subsystem inits   (LED -> trigger -> fog -> motor -> SD mount)
//   ring 1 : idle.wav      -> PSRAM  (sweeps as the bytes stream in)
//   ring 2 : fire.wav      -> PSRAM
//   ring 3 : cooldown.wav  -> PSRAM
//   ring 4 : everything after (settings + debug console; also headroom for
//            future major subsystems, e.g. a web-UI init)
// The bars fill in cyan, held at LED_BOOT_BRIGHTNESS for the whole sequence.
// On completion the entire barrel flashes a result code: green x1 = all checks
// passed, yellow x2 = recoverable / degraded (wired but not reachable yet -
// needs the planned config.txt / audio-optional path), red x2 = hard fail
// (firmware refuses to boot; the partly-filled bars stay lit as a diagnostic).
// These are boot-time-only visuals - there is no runtime setter for them, as
// the Serial console isn't up yet while they play and there is no persistence
// layer to carry a change to the next boot.
constexpr uint8_t LED_COLOR_BOOT_R      = 0x33, LED_COLOR_BOOT_G      = 0xB5, LED_COLOR_BOOT_B      = 0xE5; // cyan fill
constexpr uint8_t LED_COLOR_BOOT_OK_R   = 0x00, LED_COLOR_BOOT_OK_G   = 0xC8, LED_COLOR_BOOT_OK_B   = 0x00; // green x1
constexpr uint8_t LED_COLOR_BOOT_WARN_R = 0xE0, LED_COLOR_BOOT_WARN_G = 0xA0, LED_COLOR_BOOT_WARN_B = 0x00; // yellow x2
constexpr uint8_t LED_COLOR_BOOT_FAIL_R = 0xC8, LED_COLOR_BOOT_FAIL_G = 0x00, LED_COLOR_BOOT_FAIL_B = 0x00; // red x2
constexpr float    LED_BOOT_BRIGHTNESS   = 0.50f; // fixed for the whole sequence, fill and result flash alike
constexpr uint16_t LED_BOOT_FLASH_ON_MS  = 160;   // result-code per-flash on time (yellow / red)
constexpr uint16_t LED_BOOT_OK_FLASH_ON_MS = 500; // the single green "all clear" flash is held longer
constexpr uint16_t LED_BOOT_FLASH_OFF_MS = 160;   // gap between flashes / before the idle handoff

// ---------- Vent LEDs (2 strips, cooling vents on either side of the grip) ----------
// Simple accent lighting - no wipe or kachunk animation like the barrel.
// Idle: ventColor, breathing in sync with the barrel's idle breathing, just
// capped at its own (lower-key) peak brightness. Firing + kachunk cooldown:
// ventFireColor at ventFireBrightness (LED_VENT_FIRE_BRIGHTNESS, 30% stock),
// then back to ventColor for the fade.
// ventFireColor defaults to the same value as ventColor, so stock vents only
// ever change intensity, never hue - an alt mode can set it differently.
constexpr uint8_t  PIN_VENT_LEFT   = 41; // one side of the cooling vents (LED strip #2 on the PCB)
constexpr uint8_t  PIN_VENT_RIGHT  = 40; // other side of the cooling vents (LED strip #3 on the PCB)
constexpr uint16_t VENT_LED_COUNT  = 80; // 8 groups of 10 LEDs per side

constexpr uint8_t LED_COLOR_VENT_DEFAULT_R = 0x00, LED_COLOR_VENT_DEFAULT_G = 0x55, LED_COLOR_VENT_DEFAULT_B = 0xFF; // blue (#0055FF); also the stock ventFireColor
constexpr float   LED_VENT_MAX_BRIGHTNESS  = 0.25f; // peak of the idle breathe range (shares the barrel's min)

// ---------- Display mode (5 clicks; /DisplayMode/ on the SD card) ----------
// A tabletop "display" mode: /DisplayMode/idle.wav loops, the trigger and
// fogger are ignored, and every activityMin..Max seconds (random)
// /DisplayMode/activity.wav plays while the barrel "twitches" a random
// twitchCountMin..Max times - random direction, random length, slow speed,
// limp (EN disabled) between twitches. /DisplayMode/DisplayMode.txt holds the
// same keys as an alt mode's mode.txt plus the display keys below. ALL THREE
// files are required - if any is missing, the switch is rejected (red flash)
// and the prop reverts to stock. These are the stock defaults; the console
// and DisplayMode.txt override them.
constexpr const char* DISPLAY_MODE_DIR         = "/DisplayMode";
constexpr const char* DISPLAY_MODE_CONFIG_FILE = "DisplayMode.txt";
constexpr const char* ACTIVITY_WAV_PATH        = "/activity.wav";
constexpr float   DISPLAY_ACTIVITY_MIN_SEC     = 20.0f;
constexpr float   DISPLAY_ACTIVITY_MAX_SEC     = 60.0f;
constexpr uint8_t DISPLAY_TWITCH_COUNT_MIN     = 1;
constexpr uint8_t DISPLAY_TWITCH_COUNT_MAX     = 5;
constexpr float   DISPLAY_TWITCH_MIN_SEC       = 0.3f;  // how long each twitch drives the barrel
constexpr float   DISPLAY_TWITCH_MAX_SEC       = 1.5f;
constexpr float   DISPLAY_TWITCH_PAUSE_MIN_SEC = 0.2f;  // rest between twitches
constexpr float   DISPLAY_TWITCH_PAUSE_MAX_SEC = 1.0f;
constexpr float   DISPLAY_TWITCH_RPM           = 10.0f; // vs ~70 RPM when firing
constexpr float   DISPLAY_TWITCH_ACCEL_PERCENT = 50.0f; // % of MOTOR_ACCEL_HZ_S2 (slower to get up to speed)
constexpr float   DISPLAY_TWITCH_RPM_MAX       = 30.0f; // safety cap on twitchRPM - it moves unattended

// ---------- Status indicator (2 WS2812s, same type as the vents) ----------
// Normally just mirrors the vent strips. While a mode switch is pending/
// loading it blacks out and blinks green in the target mode's pattern:
//   AltMode1 = left LED, AltMode2 = right LED, AltMode3 = both,
//   stock    = left/right alternating, display mode = both double-blink purple.
// If the alt mode had to borrow any stock WAVs from the card root, the same
// pattern turns turquoise once loaded. It holds the pattern for at least
// STATUS_MODE_MIN_SHOW_MS (so a fast load is still readable), then returns to
// mirroring the vents. A rejected alt
// mode flashes both LEDs bright red STATUS_FAULT_FLASHES times instead. It
// also mirrors the barrel's boot result flash. GPIO39 is clear of the
// strapping pins, USB D-/D+, and the OPI PSRAM range.
constexpr uint8_t  PIN_STATUS_LED    = 39;
constexpr uint16_t STATUS_LED_COUNT  = 2;   // index 0 = left, 1 = right
constexpr uint8_t  LED_COLOR_STATUS_MODE_R  = 0x00, LED_COLOR_STATUS_MODE_G  = 0xFF, LED_COLOR_STATUS_MODE_B  = 0x00; // green
constexpr uint8_t  LED_COLOR_STATUS_FALLBACK_R = 0x00, LED_COLOR_STATUS_FALLBACK_G = 0xE0, LED_COLOR_STATUS_FALLBACK_B = 0xC0; // turquoise
constexpr uint8_t  LED_COLOR_STATUS_DISPLAY_R = 0x90, LED_COLOR_STATUS_DISPLAY_G = 0x00, LED_COLOR_STATUS_DISPLAY_B = 0xFF; // purple
constexpr uint16_t STATUS_DOUBLE_BLINK_ON_MS  = 120;  // display-mode pattern: on/off/on, then a long off
constexpr uint16_t STATUS_DOUBLE_BLINK_GAP_MS = 120;
constexpr uint16_t STATUS_DOUBLE_BLINK_REST_MS = 640;
constexpr uint8_t  LED_COLOR_STATUS_FAULT_R = 0xFF, LED_COLOR_STATUS_FAULT_G = 0x00, LED_COLOR_STATUS_FAULT_B = 0x00; // red
constexpr float    STATUS_MODE_BRIGHTNESS   = 0.60f;
constexpr float    STATUS_FAULT_BRIGHTNESS  = 1.00f; // "bright red"
constexpr uint16_t STATUS_BLINK_ON_MS       = 250;
constexpr uint16_t STATUS_BLINK_OFF_MS      = 250;
constexpr uint16_t STATUS_MODE_MIN_SHOW_MS  = 1500;
constexpr uint8_t  STATUS_FAULT_FLASHES     = 4;

// ---------- Fogger (MOSFET-switched, digital on/off only) ----------
constexpr uint8_t PIN_FOG = 9; // active HIGH into MOSFET gate
