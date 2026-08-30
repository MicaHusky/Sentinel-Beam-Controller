#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// =============================================================================
// LEDManager
//
// Drives LED_NUM_SETS groups of LED_SET_SIZE WS2812s (only the first
// LED_ACTIVE_COUNT of the physical strip are ever touched).
//
//   IDLE           : all sets pulse the idle color, breathing between
//                     idleMinBrightness and idleMaxBrightness.
//   FIRING         : brightness jumps to 100%; sets fade idle-color->fire-color
//                     one at a time over fireWipeSeconds. If firing runs
//                     longer than that, everything just stays full fire-color.
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
// states above, but with much simpler behavior: a single fixed color that
// never changes hue, only intensity - breathing at idle (same phase as the
// barrel, capped at its own lower peak), full brightness while firing/
// cooling down, and rainbow mode overrides them exactly like the barrel.
//
// Colors, wipe/breathe timing, brightness range, and the kachunk
// timestamps/tail are all RUNTIME-TUNABLE - Config.h values are only the
// boot-time defaults. This is what DebugConsole's idleColor=, fireColor=,
// fireWipeSeconds=, idleBreatheSeconds=, idleMinBrightness=,
// idleMaxBrightness=, kachunk1..kachunk5=, kachunkTimestamps=, and
// kachunkTail= commands adjust live.
//
// Like the other managers, this only reacts to notifyFireStart() /
// notifyFireEnd() calls from the main sketch - it has no idea whether audio
// or the motor are involved.
// =============================================================================

enum class LEDState {
    IDLE,
    FIRING,
    COOLDOWN_WIPE,
    COOLDOWN_FADE
};

class LEDManager {
public:
    LEDManager();

    void begin();
    void update(); // call once per loop()

    void notifyFireStart(); // trigger pressed
    void notifyFireEnd();   // trigger released, or fire.wav reached EOF

    // ---- runtime-tunable settings (each also has a getter for DebugConsole's help output) ----
    void setIdleColor(uint8_t r, uint8_t g, uint8_t b);
    void setFireColor(uint8_t r, uint8_t g, uint8_t b);
    void setFireWipeSeconds(float seconds);
    void setIdleBreatheSeconds(float seconds);
    void setIdleMinBrightness(float fraction0to1);
    void setIdleMaxBrightness(float fraction0to1);

    // Vent strips: single color, no hue change ever - only intensity moves.
    void setVentColor(uint8_t r, uint8_t g, uint8_t b);
    void getVentColor(uint8_t& r, uint8_t& g, uint8_t& b) const { r = _ventR; g = _ventG; b = _ventB; }
    void setVentMaxBrightness(float fraction0to1);
    float getVentMaxBrightness() const { return _ventMaxBrightness; }

    // Individual kachunk timestamp (index 0-4 -> kachunk 1-5), seconds from
    // the start of cooldown.wav. Must stay strictly ascending - callers
    // (DebugConsole) are expected to validate against neighbors before
    // calling this.
    void setKachunkTimestamp(uint8_t index, float seconds);
    float getKachunkTimestamp(uint8_t index) const;
    // All 5 at once.
    void setKachunkTimestamps(float t1, float t2, float t3, float t4, float t5);
    void setKachunkTailSeconds(float seconds);
    float getKachunkTailSeconds() const { return _kachunkTailSec; }

    // True for exactly one update() call, the moment the cooldown sequence's
    // last needed kachunk + tail has elapsed - the main sketch uses this to
    // know when to cut cooldown.wav and start idle.wav again.
    bool cooldownWipeComplete() const { return _cooldownWipeJustCompleted; }

    // ---- rainbow mode: a full-strip color-wheel chase that overrides the
    // normal idle/firing/cooldown rendering entirely while enabled. Purely a
    // visual override - the trigger state machine (motor/audio/fog) keeps
    // running normally underneath it. ----
    void setRainbowMode(bool enabled);
    bool isRainbowMode() const { return _rainbowMode; }
    void setRainbowSpeed(float wheelPositionsPerSecond);
    float getRainbowSpeed() const { return _rainbowSpeed; }

    void getIdleColor(uint8_t& r, uint8_t& g, uint8_t& b) const { r = _idleR; g = _idleG; b = _idleB; }
    void getFireColor(uint8_t& r, uint8_t& g, uint8_t& b) const { r = _fireR; g = _fireG; b = _fireB; }
    float getFireWipeSeconds()     const { return _fireWipeSeconds; }
    float getIdleBreatheSeconds()  const { return (float)_idleBreathePeriodMs / 1000.0f; }
    float getIdleMinBrightness()   const { return _idleMinBrightness; }
    float getIdleMaxBrightness()   const { return _idleMaxBrightness; }

private:
    void renderIdle();
    void renderFiring();
    void renderCooldownWipe();
    void renderCooldownFade();
    void renderRainbow();

    void setSetColor(uint8_t setIndex, uint8_t r, uint8_t g, uint8_t b, float brightness);
    void setVentStrips(uint8_t r, uint8_t g, uint8_t b, float brightness);
    static float segmentProgress(uint8_t index, uint8_t count, float overallProgress);
    static uint8_t lerp8(uint8_t a, uint8_t b, float t);
    uint32_t colorWheel(uint8_t wheelPos) const; // classic 0-255 RGB color wheel

    Adafruit_NeoPixel _strip;
    Adafruit_NeoPixel _ventLeft;
    Adafruit_NeoPixel _ventRight;

    LEDState _state = LEDState::IDLE;
    unsigned long _stateStartMs = 0;

    bool _rainbowMode = false;
    float _rainbowSpeed;         // wheel positions/sec
    unsigned long _rainbowStartMs = 0;

    // Per-set idle->fire mix, 0.0 = fully idle color, 1.0 = fully fire color.
    // Written during FIRING, read/mutated as COOLDOWN_WIPE hard-switches
    // each ring back to 0.0 in turn.
    float _setMix[LED_NUM_SETS] = {0};

    // How many rings actually lit up before firing ended - computed once in
    // notifyFireEnd() from _setMix[]. Only this many kachunk beats are used.
    uint8_t _neededBeats = 0;
    bool _cooldownWipeJustCompleted = false;

    // ---- runtime settings, seeded from Config.h defaults in the constructor ----
    uint8_t _idleR, _idleG, _idleB;
    uint8_t _fireR, _fireG, _fireB;
    float    _fireWipeSeconds;
    uint32_t _idleBreathePeriodMs;
    float    _idleMinBrightness;
    float    _idleMaxBrightness;
    float    _kachunkTimestampSec[5];
    float    _kachunkTailSec;

    uint8_t _ventR, _ventG, _ventB;
    float   _ventMaxBrightness;
};
