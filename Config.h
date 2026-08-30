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
constexpr const char* FIRMWARE_VERSION = "1.0.0";

// ---------- Trigger ----------
constexpr uint8_t  PIN_TRIGGER          = 4;   // INPUT_PULLUP, switch pulls to GND
constexpr uint16_t TRIGGER_DEBOUNCE_MS  = 20;

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

// ---------- Stepper (TMC2209, STEP/DIR mode) ----------
constexpr uint8_t  PIN_STEPPER_STEP   = 15;
constexpr uint8_t  PIN_STEPPER_DIR    = 16;
constexpr uint8_t  PIN_STEPPER_ENABLE = 17;  // active LOW

constexpr uint32_t MOTOR_SPEED_HZ     = 7467;   // ~70 RPM at barrel w/ 4:1 reduction
constexpr uint32_t MOTOR_ACCEL_HZ_S2  = 14934;  // ~0.5s spin-up / spin-down

// ---------- LEDs (WS2812 addressable strip) ----------
constexpr uint8_t  PIN_LED_DATA     = 18; // clear of SD/I2S/stepper, USB D-/D+ (19/20), and OPI PSRAM pins (26-32)

constexpr uint16_t LED_TOTAL_COUNT  = 160; // physical length of the bench strip - shrink this once the real strip is cut
constexpr uint16_t LED_ACTIVE_COUNT = 85;  // only the first 85 are ever addressed
constexpr uint8_t  LED_SET_SIZE     = 17;
constexpr uint8_t  LED_NUM_SETS     = LED_ACTIVE_COUNT / LED_SET_SIZE; // 5

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
constexpr float   LED_VENT_MAX_BRIGHTNESS  = 0.50f; // peak of the idle breathe range (shares the barrel's min)

// ---------- Fogger (MOSFET-switched, digital on/off only) ----------
constexpr uint8_t PIN_FOG = 9; // active HIGH into MOSFET gate
