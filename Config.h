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
constexpr const char* FIRMWARE_VERSION = "1.3.1";

// ---------- Trigger ----------
constexpr uint8_t  PIN_TRIGGER          = 4;   // INPUT_PULLUP, switch pulls to GND
constexpr uint16_t TRIGGER_DEBOUNCE_MS  = 20;

// ---------- Mode button ----------
// Second momentary button, wired exactly like the trigger (INPUT_PULLUP,
// switch shorts the pin to GND, so LOW = pressed). Each debounced press
// advances the LED mode one step and wraps around: Normal -> Rainbow -> ...
// -> Normal. GPIO1 is clear of the strapping pins (0/3/45/46), USB D-/D+
// (19/20), and the OPI PSRAM range.
constexpr uint8_t  PIN_MODE_BUTTON         = 1;
constexpr uint16_t MODE_BUTTON_DEBOUNCE_MS = 20;

// ---------- microSD (boot-time only, then fully shut down) ----------
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
constexpr float LED_IDLE_MAX_BRIGHTNESS = 0.25f;
constexpr float LED_FIRE_BRIGHTNESS     = 1.0f;

constexpr uint8_t LED_COLOR_IDLE_DEFAULT_R = 0xDF, LED_COLOR_IDLE_DEFAULT_G = 0x57, LED_COLOR_IDLE_DEFAULT_B = 0x09;
constexpr uint8_t LED_COLOR_FIRE_DEFAULT_R = 0x64, LED_COLOR_FIRE_DEFAULT_G = 0x70, LED_COLOR_FIRE_DEFAULT_B = 0xE2;

constexpr float LED_RAINBOW_SPEED_DEFAULT = 40.0f; // color-wheel positions/sec the chase advances by

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
constexpr uint16_t LED_BOOT_FLASH_ON_MS  = 160;   // result-code per-flash on time
constexpr uint16_t LED_BOOT_FLASH_OFF_MS = 160;   // gap between flashes / before the idle handoff

// ---------- Vent LEDs (2 strips, cooling vents on either side of the grip) ----------
// Purely single-color/single-brightness accent lighting - no wipe or kachunk
// animation like the barrel. Idle: breathes in sync with the barrel's idle
// breathing, just capped at its own (lower-key) peak brightness. Firing:
// jumps to full brightness, same color - only intensity changes, never hue.
// Rainbow mode overrides both of these exactly like it does the barrel.
constexpr uint8_t  PIN_VENT_LEFT   = 41; // one side of the cooling vents (LED strip #2 on the PCB)
constexpr uint8_t  PIN_VENT_RIGHT  = 40; // other side of the cooling vents (LED strip #3 on the PCB)
constexpr uint16_t VENT_LED_COUNT  = 80; // 8 groups of 10 LEDs per side

constexpr uint8_t LED_COLOR_VENT_DEFAULT_R = 0xE8, LED_COLOR_VENT_DEFAULT_G = 0xA0, LED_COLOR_VENT_DEFAULT_B = 0x20; // slightly orangey-yellow
constexpr float   LED_VENT_MAX_BRIGHTNESS  = 0.25f; // peak of the idle breathe range (shares the barrel's min)

// ---------- Fogger (MOSFET-switched, digital on/off only) ----------
constexpr uint8_t PIN_FOG = 9; // active HIGH into MOSFET gate
