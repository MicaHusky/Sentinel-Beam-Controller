#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// =============================================================================
// LEDManager
//
// Drives the barrel strip: LED_NUM_SETS rings of LED_SET_SIZE WS2812s
// (LED_COUNT total).
//
//   IDLE           : all sets pulse the idle color, breathing between
//                     idleMinBrightness and idleMaxBrightness.
//   FIRING         : brightness jumps to 100%; sets fade idle-color->fire-color
//                     one at a time over fireWipeSeconds. If firing runs
//                     longer than that, everything just stays full fire-color.
//                     A rainbow fire color skips the wipe: the whole barrel
//                     erupts into rainbow the instant the trigger is pulled.
//   COOLDOWN_WIPE  : rings HARD-SWITCH back to the idle color (no fade, an
//                     instant cut) in reverse order, each cued by one of 5
//                     manually-tuned "kachunk" timestamps rather than a fixed
//                     duration - only as many kachunks as rings actually need
//                     reverting are used, so releasing early naturally uses
//                     fewer beats and finishes sooner. Any ring beyond that
//                     count was never lit and needs no beat at all. A tail
//                     time is held after the last needed kachunk before
//                     moving on, to let that hit's decay finish naturally.
//   COOLDOWN_FADE  : once every needed ring has hard-switched back, brightness
//                     eases from 100% down to the idle level over
//                     LED_BRIGHTNESS_FADE_MS, then hands off to IDLE breathing.
//
// COOLDOWN_WIPE runs on this object's own millis()-based clock (set the
// instant notifyFireEnd() is called) rather than reading AudioManager's
// playback position - since the main sketch starts cooldown.wav and calls
// notifyFireEnd() back-to-back in the same loop() iteration, the two clocks
// stay close enough (sub-millisecond) for kachunk sync purposes. This keeps
// LEDManager fully independent of AudioManager, same as every other manager.
//
// Two vent LED strips (left/right cooling vents) ride along with all four
// states above, with much simpler behavior: ventColor breathing at idle (same
// phase as the barrel, capped at its own lower peak), ventFireColor at full
// brightness while firing and through the kachunk wipe, then ventColor again
// for the fade back to idle. Stock ventFireColor == ventColor, so stock vents
// only ever change intensity.
//
// Any of the four color slots (idle / fire / vent / ventFire) can be "rainbow"
// instead of a fixed color: that slot renders a color-wheel chase at a fixed
// rainbowBrightness (rainbow never breathes).
//
// A 2-LED status indicator normally mirrors the vents. The main sketch can
// take it over to show a mode-switch pattern (green) or a fault (red) - see
// showModeIndicator() / showFaultIndicator().
//
// Colors, wipe/breathe timing, brightness range, rainbow speed/brightness,
// and the kachunk timestamps/tail are all RUNTIME-TUNABLE (debug console or
// an alt mode's mode.txt) - Config.h values are only the stock defaults, which
// resetToDefaults() restores.
//
// Like the other managers, this only reacts to calls from the main sketch -
// it has no idea whether audio or the motor are involved.
// =============================================================================

enum class LEDState {
    BOOT,
    IDLE,
    FIRING,
    COOLDOWN_WIPE,
    COOLDOWN_FADE
};

// Result code the barrel (and status indicator) flashes once the boot sequence
// finishes (see beginBootSequence()): GREEN x1 / YELLOW x2 / RED x2.
// RECOVERABLE is wired through but not reachable yet - it needs the planned
// config.txt / audio-optional path that lets the prop run in a degraded mode.
enum class BootResult { OK, RECOVERABLE, HARD_FAIL };

enum class ColorSlot : uint8_t {
    IDLE = 0,  // barrel at idle
    FIRE,      // barrel once wiped during firing
    VENT,      // vents at idle / cooldown fade
    VENT_FIRE, // vents while firing + kachunk wipe
    COUNT
};

struct SlotColor {
    uint8_t r = 0, g = 0, b = 0;
    bool rainbow = false;
};

class LEDManager {
public:
    LEDManager();

    void begin();
    void update(); // call once per loop()

    void notifyFireStart(); // trigger pressed
    void notifyFireEnd();   // trigger released, or fire.wav reached EOF

    // ---- boot progress bar -------------------------------------------------
    // The 5 barrel rings each act as an independent 17-LED loading bar for one
    // boot phase (ring 0 = subsystem inits, rings 1-3 = the three WAV loads,
    // ring 4 = everything after). Driven synchronously by the main sketch from
    // setup(), before loop()/update() run, so each of these renders and
    // show()s immediately. The vent strips and status indicator are held dark
    // for the whole sequence. Colors/brightness/flash timing are Config.h
    // boot-time constants - there is deliberately no runtime setter.
    void beginBootSequence();
    void setBootRingProgress(uint8_t ring, float fraction0to1);
    void bootFlash(BootResult result); // BLOCKING (~0.3-0.7s of delay()); leaves the bars lit
    void endBootSequence();             // hand off to IDLE breathing

    // ---- status indicator (2 LEDs on PIN_STATUS_LED) ----
    // profileIndex: 0 = stock (left/right alternating), 1..3 = AltMode1..3
    // (left / right / both). Blinks green until releaseModeIndicator().
    void showModeIndicator(uint8_t profileIndex);
    // Back to mirroring the vents - but not before the pattern has been shown
    // for STATUS_MODE_MIN_SHOW_MS, so a fast load is still readable.
    // usedRootSounds: the alt mode borrowed stock WAVs from the card root -
    // the same pattern switches to turquoise and gets a full
    // STATUS_MODE_MIN_SHOW_MS of its own before releasing.
    void releaseModeIndicator(bool usedRootSounds = false);
    // Bright red flashes on both LEDs, then back to mirroring the vents.
    void showFaultIndicator();

    // ---- runtime-tunable settings ----
    void resetToDefaults(); // everything below back to the Config.h stock values

    void setColor(ColorSlot slot, uint8_t r, uint8_t g, uint8_t b);
    void setColorRainbow(ColorSlot slot);
    SlotColor getColor(ColorSlot slot) const;

    void setFireWipeSeconds(float seconds);
    void setIdleBreatheSeconds(float seconds);
    void setIdleMinBrightness(float fraction0to1);
    void setIdleMaxBrightness(float fraction0to1);
    void setVentMaxBrightness(float fraction0to1);
    float getVentMaxBrightness() const { return _ventMaxBrightness; }

    // Individual kachunk timestamp (index 0-4 -> kachunk 1-5), seconds from
    // the start of cooldown.wav. Must stay strictly ascending - callers
    // (SettingsController) are expected to validate against neighbors before
    // calling this.
    void setKachunkTimestamp(uint8_t index, float seconds);
    float getKachunkTimestamp(uint8_t index) const;
    // All 5 at once.
    void setKachunkTimestamps(float t1, float t2, float t3, float t4, float t5);
    void setKachunkTailSeconds(float seconds);
    float getKachunkTailSeconds() const { return _kachunkTailSec; }

    void setRainbowSpeed(float wheelPositionsPerSecond);
    float getRainbowSpeed() const { return _rainbowSpeed; }
    void setRainbowBrightness(float fraction0to1);
    float getRainbowBrightness() const { return _rainbowBrightness; }

    // True for exactly one update() call, the moment the cooldown sequence's
    // last needed kachunk + tail has elapsed - the main sketch uses this to
    // know when to cut cooldown.wav and start idle.wav again.
    bool cooldownWipeComplete() const { return _cooldownWipeJustCompleted; }

    float getFireWipeSeconds()     const { return _fireWipeSeconds; }
    float getIdleBreatheSeconds()  const { return (float)_idleBreathePeriodMs / 1000.0f; }
    float getIdleMinBrightness()   const { return _idleMinBrightness; }
    float getIdleMaxBrightness()   const { return _idleMaxBrightness; }

private:
    enum class StatusMode { FOLLOW_VENTS, MODE_PATTERN, FAULT };

    void renderBoot();
    void renderIdle();
    void renderFiring();
    void renderCooldownWipe();
    void renderCooldownFade();
    void renderStatus();

    // Barrel: per ring, blend the idle slot (at idleHexBrightness) toward the
    // fire slot (at fireHexBrightness) by mix[ring]. Rainbow slots ignore the
    // hex brightness and use rainbowBrightness.
    void renderBarrel(const float* mix, float idleHexBrightness, float fireHexBrightness);
    void renderVents(const SlotColor& c, float hexBrightness);
    void slotPixel(const SlotColor& c, uint16_t i, uint16_t count, float hexBrightness,
                   uint8_t& r, uint8_t& g, uint8_t& b) const;
    void setStatusPixels(uint32_t left, uint32_t right);

    static float segmentProgress(uint8_t index, uint8_t count, float overallProgress);
    static uint8_t lerp8(uint8_t a, uint8_t b, float t);
    static void wheelRGB(uint8_t wheelPos, uint8_t& r, uint8_t& g, uint8_t& b); // classic 0-255 color wheel

    Adafruit_NeoPixel _strip;
    Adafruit_NeoPixel _ventLeft;
    Adafruit_NeoPixel _ventRight;
    Adafruit_NeoPixel _status;

    LEDState _state = LEDState::IDLE;
    unsigned long _stateStartMs = 0;

    // Rainbow chase position for the current frame (0..256), computed once per update().
    float _rainbowBase = 0.0f;

    // Per-set idle->fire mix, 0.0 = fully idle color, 1.0 = fully fire color.
    // Written during FIRING, read/mutated as COOLDOWN_WIPE hard-switches
    // each ring back to 0.0 in turn.
    float _setMix[LED_NUM_SETS] = {0};

    // How many rings actually lit up before firing ended - computed once in
    // notifyFireEnd() from _setMix[]. Only this many kachunk beats are used.
    uint8_t _neededBeats = 0;
    bool _cooldownWipeJustCompleted = false;

    // Per-ring fill for the boot progress bar, 0.0..1.0. Only meaningful while
    // _state == LEDState::BOOT.
    float _bootRingProgress[LED_NUM_SETS] = {0};

    // ---- status indicator ----
    StatusMode    _statusMode = StatusMode::FOLLOW_VENTS;
    uint8_t       _statusProfile = 0;
    bool          _statusFallback = false; // turquoise instead of green
    unsigned long _statusStartMs = 0;
    unsigned long _statusReleaseAtMs = 0; // 0 = hold until released

    // ---- runtime settings (see resetToDefaults()) ----
    SlotColor _colors[(uint8_t)ColorSlot::COUNT];
    float    _fireWipeSeconds;
    uint32_t _idleBreathePeriodMs;
    float    _idleMinBrightness;
    float    _idleMaxBrightness;
    float    _kachunkTimestampSec[5];
    float    _kachunkTailSec;
    float    _ventMaxBrightness;
    float    _rainbowSpeed;      // wheel positions/sec
    float    _rainbowBrightness; // 0..1, fixed (rainbow never breathes)
};
