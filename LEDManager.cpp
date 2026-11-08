#include "LEDManager.h"
#include "Config.h"

LEDManager::LEDManager()
    : _strip(LED_COUNT, PIN_LED_DATA, NEO_GRB + NEO_KHZ800),
      _ventLeft(VENT_LED_COUNT, PIN_VENT_LEFT, NEO_GRB + NEO_KHZ800),
      _ventRight(VENT_LED_COUNT, PIN_VENT_RIGHT, NEO_GRB + NEO_KHZ800),
      _status(STATUS_LED_COUNT, PIN_STATUS_LED, NEO_GRB + NEO_KHZ800) {
    resetToDefaults();
}

void LEDManager::resetToDefaults() {
    SlotColor idle, fire, vent;
    idle.r = LED_COLOR_IDLE_DEFAULT_R; idle.g = LED_COLOR_IDLE_DEFAULT_G; idle.b = LED_COLOR_IDLE_DEFAULT_B;
    fire.r = LED_COLOR_FIRE_DEFAULT_R; fire.g = LED_COLOR_FIRE_DEFAULT_G; fire.b = LED_COLOR_FIRE_DEFAULT_B;
    vent.r = LED_COLOR_VENT_DEFAULT_R; vent.g = LED_COLOR_VENT_DEFAULT_G; vent.b = LED_COLOR_VENT_DEFAULT_B;
    _colors[(uint8_t)ColorSlot::IDLE]      = idle;
    _colors[(uint8_t)ColorSlot::FIRE]      = fire;
    _colors[(uint8_t)ColorSlot::VENT]      = vent;
    _colors[(uint8_t)ColorSlot::VENT_FIRE] = vent; // stock: vents never change hue

    _fireWipeSeconds     = LED_FIRE_WIPE_SECONDS;
    _idleBreathePeriodMs = LED_IDLE_BREATHE_PERIOD_MS;
    _idleMinBrightness   = LED_IDLE_MIN_BRIGHTNESS;
    _idleMaxBrightness   = LED_IDLE_MAX_BRIGHTNESS;
    _kachunkTimestampSec[0] = LED_KACHUNK_1_SEC;
    _kachunkTimestampSec[1] = LED_KACHUNK_2_SEC;
    _kachunkTimestampSec[2] = LED_KACHUNK_3_SEC;
    _kachunkTimestampSec[3] = LED_KACHUNK_4_SEC;
    _kachunkTimestampSec[4] = LED_KACHUNK_5_SEC;
    _kachunkTailSec      = LED_KACHUNK_TAIL_SEC;
    _ventMaxBrightness   = LED_VENT_MAX_BRIGHTNESS;
    _fireBrightness      = LED_FIRE_BRIGHTNESS;
    _ventFireBrightness  = LED_VENT_FIRE_BRIGHTNESS;
    _rainbowSpeed        = LED_RAINBOW_SPEED_DEFAULT;
    _rainbowBrightness   = LED_RAINBOW_BRIGHTNESS_DEFAULT;
}

void LEDManager::begin() {
    _strip.begin();
    _strip.clear();
    _strip.show();

    _ventLeft.begin();
    _ventLeft.clear();
    _ventLeft.show();

    _ventRight.begin();
    _ventRight.clear();
    _ventRight.show();

    _status.begin();
    _status.clear();
    _status.show();

    for (uint8_t i = 0; i < LED_NUM_SETS; i++) {
        _setMix[i] = 0.0f;
    }

    _state = LEDState::IDLE;
    _stateStartMs = millis();
    _statusMode = StatusMode::FOLLOW_VENTS;

    Serial.printf("[LEDManager] initialized: %u rings of %u LEDs (%u total) + 2 vent strips of %u + %u status LEDs\n",
                  LED_NUM_SETS, LED_SET_SIZE, LED_COUNT, VENT_LED_COUNT, STATUS_LED_COUNT);
}

void LEDManager::notifyFireStart() {
    _state = LEDState::FIRING;
    _stateStartMs = millis();
    if (_colors[(uint8_t)ColorSlot::FIRE].rainbow) {
        Serial.println("[LEDManager] -> FIRING (rainbow fire color - whole barrel switches at once)");
    } else {
        Serial.println("[LEDManager] -> FIRING (idle-color->fire-color wipe begins)");
    }
}

void LEDManager::notifyFireEnd() {
    // How many rings actually need reverting - only the ones that lit up at
    // all before firing ended. This determines how many kachunk beats get
    // used (1-5), which in turn determines how much of cooldown.wav plays.
    _neededBeats = 0;
    for (uint8_t i = 0; i < LED_NUM_SETS; i++) {
        if (_setMix[i] > 0.0f) _neededBeats++;
    }

    _state = LEDState::COOLDOWN_WIPE;
    _stateStartMs = millis(); // same wall-clock reference cooldown.wav effectively starts at
    _cooldownWipeJustCompleted = false;

    Serial.printf("[LEDManager] -> COOLDOWN_WIPE (%u ring(s) need reverting)\n", _neededBeats);
}

// ---- boot progress bar ------------------------------------------------------

void LEDManager::beginBootSequence() {
    _state = LEDState::BOOT;
    for (uint8_t i = 0; i < LED_NUM_SETS; i++) _bootRingProgress[i] = 0.0f;

    _strip.clear();
    _strip.show();
    _ventLeft.clear();  _ventLeft.show();   // vents + status stay dark for the whole boot sequence
    _ventRight.clear(); _ventRight.show();
    _status.clear();    _status.show();

    Serial.println("[LEDManager] -> BOOT (barrel rings = 5-phase loading bar)");
}

void LEDManager::setBootRingProgress(uint8_t ring, float fraction0to1) {
    if (ring >= LED_NUM_SETS) return;
    if (fraction0to1 < 0.0f) fraction0to1 = 0.0f;
    if (fraction0to1 > 1.0f) fraction0to1 = 1.0f;
    _bootRingProgress[ring] = fraction0to1;
    if (_state == LEDState::BOOT) renderBoot(); // setup() drives this synchronously - paint now
}

void LEDManager::renderBoot() {
    const uint32_t lit = _strip.Color((uint8_t)(LED_COLOR_BOOT_R * LED_BOOT_BRIGHTNESS),
                                      (uint8_t)(LED_COLOR_BOOT_G * LED_BOOT_BRIGHTNESS),
                                      (uint8_t)(LED_COLOR_BOOT_B * LED_BOOT_BRIGHTNESS));

    for (uint8_t s = 0; s < LED_NUM_SETS; s++) {
        uint16_t litCount = (uint16_t)(_bootRingProgress[s] * (float)LED_SET_SIZE + 0.5f);
        uint16_t startLed = (uint16_t)s * LED_SET_SIZE;
        for (uint16_t i = 0; i < LED_SET_SIZE; i++) {
            _strip.setPixelColor(startLed + i, i < litCount ? lit : (uint32_t)0);
        }
    }
    _strip.show();
}

void LEDManager::bootFlash(BootResult result) {
    uint8_t r, g, b, count;
    uint16_t onMs = LED_BOOT_FLASH_ON_MS;
    const char* label;
    switch (result) {
        case BootResult::OK:
            r = LED_COLOR_BOOT_OK_R;   g = LED_COLOR_BOOT_OK_G;   b = LED_COLOR_BOOT_OK_B;
            count = 1; onMs = LED_BOOT_OK_FLASH_ON_MS; label = "GREEN x1 (all checks passed)";
            break;
        case BootResult::RECOVERABLE:
            r = LED_COLOR_BOOT_WARN_R; g = LED_COLOR_BOOT_WARN_G; b = LED_COLOR_BOOT_WARN_B;
            count = 2; label = "YELLOW x2 (recoverable - degraded operation)";
            break;
        default: // HARD_FAIL
            r = LED_COLOR_BOOT_FAIL_R; g = LED_COLOR_BOOT_FAIL_G; b = LED_COLOR_BOOT_FAIL_B;
            count = 2; label = "RED x2 (hard fail - refusing to boot)";
            break;
    }

    const uint32_t on = _strip.Color((uint8_t)(r * LED_BOOT_BRIGHTNESS),
                                     (uint8_t)(g * LED_BOOT_BRIGHTNESS),
                                     (uint8_t)(b * LED_BOOT_BRIGHTNESS));

    for (uint8_t f = 0; f < count; f++) {
        for (uint16_t i = 0; i < LED_COUNT; i++) _strip.setPixelColor(i, on);
        _strip.show();
        setStatusPixels(on, on); // status indicator mirrors the result flash
        _status.show();
        delay(onMs);
        _strip.clear();
        _strip.show();
        _status.clear();
        _status.show();
        delay(LED_BOOT_FLASH_OFF_MS);
    }

    // On a non-OK result, leave the frozen bars lit as a diagnostic (the
    // unfilled ring points at the subsystem that failed). On OK the barrel
    // stays dark here and the caller hands straight off to idle breathing.
    if (result != BootResult::OK) renderBoot();

    Serial.printf("[LEDManager] boot result flash: %s\n", label);
}

void LEDManager::endBootSequence() {
    _state = LEDState::IDLE;
    _stateStartMs = millis();
    _statusMode = StatusMode::FOLLOW_VENTS;
    Serial.println("[LEDManager] -> IDLE (breathing)");
}

// ---- status indicator ---------------------------------------------------------

void LEDManager::showModeIndicator(uint8_t profileIndex) {
    _statusMode = StatusMode::MODE_PATTERN;
    _statusProfile = profileIndex;
    _statusFallback = false;
    _statusStartMs = millis();
    _statusReleaseAtMs = 0; // hold until releaseModeIndicator()
    renderStatus();
    _status.show(); // paint now - the caller may be about to block on the SD card
}

void LEDManager::releaseModeIndicator(bool usedRootSounds) {
    if (_statusMode != StatusMode::MODE_PATTERN) return;
    if (usedRootSounds) {
        _statusFallback = true;
        _statusStartMs = millis(); // restart, so the turquoise pattern is readable on its own
    }
    unsigned long earliest = _statusStartMs + STATUS_MODE_MIN_SHOW_MS;
    unsigned long now = millis();
    _statusReleaseAtMs = ((long)(earliest - now) > 0) ? earliest : now;
    if (_statusReleaseAtMs == 0) _statusReleaseAtMs = 1; // 0 means "hold"
}

void LEDManager::showFaultIndicator() {
    _statusMode = StatusMode::FAULT;
    _statusStartMs = millis();
    renderStatus();
    _status.show();
}

void LEDManager::setStatusPixels(uint32_t left, uint32_t right) {
    _status.setPixelColor(0, left);
    if (STATUS_LED_COUNT > 1) _status.setPixelColor(1, right);
}

void LEDManager::renderStatus() {
    const unsigned long now = millis();
    const uint32_t cycleMs = (uint32_t)STATUS_BLINK_ON_MS + STATUS_BLINK_OFF_MS;

    if (_statusMode == StatusMode::MODE_PATTERN && _statusReleaseAtMs != 0 &&
        (long)(now - _statusReleaseAtMs) >= 0) {
        _statusMode = StatusMode::FOLLOW_VENTS;
        Serial.println("[LEDManager] status indicator -> following vents");
    }

    if (_statusMode == StatusMode::FAULT) {
        unsigned long elapsed = now - _statusStartMs;
        if (elapsed >= (unsigned long)cycleMs * STATUS_FAULT_FLASHES) {
            _statusMode = StatusMode::FOLLOW_VENTS;
            Serial.println("[LEDManager] status indicator -> following vents");
        } else {
            bool lit = (elapsed % cycleMs) < STATUS_BLINK_ON_MS;
            uint32_t red = lit ? _status.Color((uint8_t)(LED_COLOR_STATUS_FAULT_R * STATUS_FAULT_BRIGHTNESS),
                                               (uint8_t)(LED_COLOR_STATUS_FAULT_G * STATUS_FAULT_BRIGHTNESS),
                                               (uint8_t)(LED_COLOR_STATUS_FAULT_B * STATUS_FAULT_BRIGHTNESS))
                               : 0;
            setStatusPixels(red, red);
            return;
        }
    }

    if (_statusMode == StatusMode::MODE_PATTERN && _statusProfile == 4) {
        // Display mode: both LEDs purple, on-off-on then a long rest.
        const uint32_t on = STATUS_DOUBLE_BLINK_ON_MS, gap = STATUS_DOUBLE_BLINK_GAP_MS;
        const uint32_t period = on + gap + on + STATUS_DOUBLE_BLINK_REST_MS;
        uint32_t t = (now - _statusStartMs) % period;
        bool lit = (t < on) || (t >= on + gap && t < on + gap + on);
        uint32_t purple = lit ? _status.Color((uint8_t)(LED_COLOR_STATUS_DISPLAY_R * STATUS_MODE_BRIGHTNESS),
                                              (uint8_t)(LED_COLOR_STATUS_DISPLAY_G * STATUS_MODE_BRIGHTNESS),
                                              (uint8_t)(LED_COLOR_STATUS_DISPLAY_B * STATUS_MODE_BRIGHTNESS))
                              : 0;
        setStatusPixels(purple, purple);
        return;
    }

    if (_statusMode == StatusMode::MODE_PATTERN) {
        unsigned long elapsed = now - _statusStartMs;
        bool lit = (elapsed % cycleMs) < STATUS_BLINK_ON_MS;
        const uint8_t cr = _statusFallback ? LED_COLOR_STATUS_FALLBACK_R : LED_COLOR_STATUS_MODE_R;
        const uint8_t cg = _statusFallback ? LED_COLOR_STATUS_FALLBACK_G : LED_COLOR_STATUS_MODE_G;
        const uint8_t cb = _statusFallback ? LED_COLOR_STATUS_FALLBACK_B : LED_COLOR_STATUS_MODE_B;
        uint32_t green = _status.Color((uint8_t)(cr * STATUS_MODE_BRIGHTNESS),
                                       (uint8_t)(cg * STATUS_MODE_BRIGHTNESS),
                                       (uint8_t)(cb * STATUS_MODE_BRIGHTNESS));
        bool left = false, right = false;
        switch (_statusProfile) {
            case 1:  left = true;               break; // AltMode1: left
            case 2:  right = true;              break; // AltMode2: right
            case 3:  left = true; right = true; break; // AltMode3: both
            default: {                                  // stock: left/right alternating
                bool leftTurn = ((elapsed / cycleMs) % 2) == 0;
                left = leftTurn; right = !leftTurn;
                break;
            }
        }
        setStatusPixels((lit && left) ? green : 0, (lit && right) ? green : 0);
        return;
    }

    // FOLLOW_VENTS: mirror the first pixels of the left vent strip.
    for (uint16_t i = 0; i < STATUS_LED_COUNT; i++) {
        _status.setPixelColor(i, _ventLeft.getPixelColor(i));
    }
}

// ---- settings -----------------------------------------------------------------

void LEDManager::setColor(ColorSlot slot, uint8_t r, uint8_t g, uint8_t b) {
    if (slot >= ColorSlot::COUNT) return;
    SlotColor& c = _colors[(uint8_t)slot];
    c.r = r; c.g = g; c.b = b; c.rainbow = false;
}

void LEDManager::setColorRainbow(ColorSlot slot) {
    if (slot >= ColorSlot::COUNT) return;
    _colors[(uint8_t)slot].rainbow = true;
}

SlotColor LEDManager::getColor(ColorSlot slot) const {
    if (slot >= ColorSlot::COUNT) return SlotColor();
    return _colors[(uint8_t)slot];
}

void LEDManager::setFireWipeSeconds(float seconds) {
    _fireWipeSeconds = seconds;
}

void LEDManager::setIdleBreatheSeconds(float seconds) {
    _idleBreathePeriodMs = (uint32_t)(seconds * 1000.0f);
}

void LEDManager::setIdleMinBrightness(float fraction0to1) {
    _idleMinBrightness = fraction0to1;
}

void LEDManager::setIdleMaxBrightness(float fraction0to1) {
    _idleMaxBrightness = fraction0to1;
}

void LEDManager::setVentMaxBrightness(float fraction0to1) {
    _ventMaxBrightness = fraction0to1;
}

void LEDManager::setKachunkTimestamp(uint8_t index, float seconds) {
    if (index > 4) return;
    _kachunkTimestampSec[index] = seconds;
}

float LEDManager::getKachunkTimestamp(uint8_t index) const {
    if (index > 4) return 0.0f;
    return _kachunkTimestampSec[index];
}

void LEDManager::setKachunkTimestamps(float t1, float t2, float t3, float t4, float t5) {
    _kachunkTimestampSec[0] = t1;
    _kachunkTimestampSec[1] = t2;
    _kachunkTimestampSec[2] = t3;
    _kachunkTimestampSec[3] = t4;
    _kachunkTimestampSec[4] = t5;
}

void LEDManager::setKachunkTailSeconds(float seconds) {
    _kachunkTailSec = seconds;
}

void LEDManager::setRainbowSpeed(float wheelPositionsPerSecond) {
    _rainbowSpeed = wheelPositionsPerSecond;
}

void LEDManager::setFireBrightness(float fraction0to1) {
    _fireBrightness = fraction0to1;
}

void LEDManager::setVentFireBrightness(float fraction0to1) {
    _ventFireBrightness = fraction0to1;
}

void LEDManager::setRainbowBrightness(float fraction0to1) {
    _rainbowBrightness = fraction0to1;
}

// ---- rendering ----------------------------------------------------------------

void LEDManager::update() {
    _cooldownWipeJustCompleted = false; // one-shot flag - reset every call, set true only the frame it fires

    // One shared chase position per frame, so every rainbow slot moves together.
    _rainbowBase = fmodf(((float)millis() / 1000.0f) * _rainbowSpeed, 256.0f);

    switch (_state) {
        case LEDState::BOOT:           renderBoot();          break;
        case LEDState::IDLE:           renderIdle();          break;
        case LEDState::FIRING:         renderFiring();        break;
        case LEDState::COOLDOWN_WIPE:  renderCooldownWipe();  break;
        case LEDState::COOLDOWN_FADE:  renderCooldownFade();  break;
    }
    renderStatus(); // after the vents, so FOLLOW_VENTS mirrors this frame

    _strip.show();
    _ventLeft.show();
    _ventRight.show();
    _status.show();
}

float LEDManager::segmentProgress(uint8_t index, uint8_t count, float overallProgress) {
    // Set `index` starts moving once overallProgress passes index/count, and
    // finishes moving by (index+1)/count - i.e. sets animate one at a time,
    // in order, evenly dividing the total duration.
    float p = overallProgress * (float)count - (float)index;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    return p;
}

uint8_t LEDManager::lerp8(uint8_t a, uint8_t b, float t) {
    return (uint8_t)((float)a + ((float)b - (float)a) * t);
}

void LEDManager::wheelRGB(uint8_t wheelPos, uint8_t& r, uint8_t& g, uint8_t& b) {
    // Classic 0-255 RGB color wheel (red -> green -> blue -> red).
    wheelPos = 255 - wheelPos;
    if (wheelPos < 85) {
        r = 255 - wheelPos * 3; g = 0; b = wheelPos * 3;
    } else if (wheelPos < 170) {
        wheelPos -= 85;
        r = 0; g = wheelPos * 3; b = 255 - wheelPos * 3;
    } else {
        wheelPos -= 170;
        r = wheelPos * 3; g = 255 - wheelPos * 3; b = 0;
    }
}

void LEDManager::slotPixel(const SlotColor& c, uint16_t i, uint16_t count, float hexBrightness,
                           uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (c.rainbow) {
        float pos = fmodf(_rainbowBase + ((float)i * 256.0f / (float)count), 256.0f);
        wheelRGB((uint8_t)pos, r, g, b);
        r = (uint8_t)((float)r * _rainbowBrightness);
        g = (uint8_t)((float)g * _rainbowBrightness);
        b = (uint8_t)((float)b * _rainbowBrightness);
    } else {
        r = (uint8_t)((float)c.r * hexBrightness);
        g = (uint8_t)((float)c.g * hexBrightness);
        b = (uint8_t)((float)c.b * hexBrightness);
    }
}

void LEDManager::renderBarrel(const float* mix, float idleHexBrightness, float fireHexBrightness) {
    const SlotColor& idle = _colors[(uint8_t)ColorSlot::IDLE];
    const SlotColor& fire = _colors[(uint8_t)ColorSlot::FIRE];

    for (uint8_t s = 0; s < LED_NUM_SETS; s++) {
        const float m = mix ? mix[s] : 0.0f;
        const uint16_t startLed = (uint16_t)s * LED_SET_SIZE;
        for (uint16_t k = 0; k < LED_SET_SIZE; k++) {
            const uint16_t i = startLed + k;
            uint8_t ir, ig, ib, fr, fg, fb;
            slotPixel(idle, i, LED_COUNT, idleHexBrightness, ir, ig, ib);
            if (m <= 0.0f) {
                _strip.setPixelColor(i, ir, ig, ib);
                continue;
            }
            slotPixel(fire, i, LED_COUNT, fireHexBrightness, fr, fg, fb);
            _strip.setPixelColor(i, lerp8(ir, fr, m), lerp8(ig, fg, m), lerp8(ib, fb, m));
        }
    }
}

void LEDManager::renderVents(const SlotColor& c, float hexBrightness) {
    for (uint16_t i = 0; i < VENT_LED_COUNT; i++) {
        uint8_t r, g, b;
        slotPixel(c, i, VENT_LED_COUNT, hexBrightness, r, g, b);
        _ventLeft.setPixelColor(i, r, g, b);
        _ventRight.setPixelColor(i, r, g, b);
    }
}

void LEDManager::renderIdle() {
    unsigned long elapsed = (millis() - _stateStartMs) % _idleBreathePeriodMs;
    float phase = (2.0f * PI * (float)elapsed) / (float)_idleBreathePeriodMs;
    float breathe = (sinf(phase) * 0.5f) + 0.5f; // 0..1

    float brightness = _idleMinBrightness + (_idleMaxBrightness - _idleMinBrightness) * breathe;
    float ventBrightness = _idleMinBrightness + (_ventMaxBrightness - _idleMinBrightness) * breathe;

    renderBarrel(nullptr, brightness, brightness);
    renderVents(_colors[(uint8_t)ColorSlot::VENT], ventBrightness);
}

void LEDManager::renderFiring() {
    unsigned long elapsed = millis() - _stateStartMs;
    float overallProgress = (float)elapsed / (_fireWipeSeconds * 1000.0f);
    if (overallProgress > 1.0f) overallProgress = 1.0f;
    // Rainbow fire color: the whole barrel erupts at once instead of wiping.
    if (_colors[(uint8_t)ColorSlot::FIRE].rainbow) overallProgress = 1.0f;

    for (uint8_t i = 0; i < LED_NUM_SETS; i++) {
        _setMix[i] = segmentProgress(i, LED_NUM_SETS, overallProgress); // set 0 wipes first
    }
    renderBarrel(_setMix, _fireBrightness, _fireBrightness);
    renderVents(_colors[(uint8_t)ColorSlot::VENT_FIRE], _ventFireBrightness);
}

void LEDManager::renderCooldownWipe() {
    if (_neededBeats == 0) {
        // Nothing lit up before release - nothing to revert, skip straight to the fade.
        _state = LEDState::COOLDOWN_FADE;
        _stateStartMs = millis();
        _cooldownWipeJustCompleted = true;
        Serial.println("[LEDManager] -> COOLDOWN_FADE (no rings were lit, skipping wipe)");
        renderCooldownFade();
        return;
    }

    unsigned long elapsed = millis() - _stateStartMs;

    // Beat 0 (the first kachunk heard) reverts the highest-index lit ring;
    // each subsequent beat works inward. Only _neededBeats kachunks are used
    // at all - the rest of cooldown.wav, if any, is simply never reached.
    for (uint8_t beat = 0; beat < _neededBeats; beat++) {
        uint8_t setIndex = _neededBeats - 1 - beat;
        uint32_t thresholdMs = (uint32_t)(_kachunkTimestampSec[beat] * 1000.0f);
        if (elapsed >= thresholdMs) {
            _setMix[setIndex] = 0.0f; // this beat's kachunk has landed - hard switch, no fade
        }
        // else: leave it exactly as frozen at the moment firing ended
    }

    renderBarrel(_setMix, _fireBrightness, _fireBrightness);
    renderVents(_colors[(uint8_t)ColorSlot::VENT_FIRE], _ventFireBrightness);

    uint32_t lastBeatMs = (uint32_t)(_kachunkTimestampSec[_neededBeats - 1] * 1000.0f);
    uint32_t cutoffMs = lastBeatMs + (uint32_t)(_kachunkTailSec * 1000.0f);

    if (elapsed >= cutoffMs) {
        for (uint8_t i = 0; i < LED_NUM_SETS; i++) _setMix[i] = 0.0f; // safety: force fully idle
        _state = LEDState::COOLDOWN_FADE;
        _stateStartMs = millis();
        _cooldownWipeJustCompleted = true;
        Serial.println("[LEDManager] -> COOLDOWN_FADE (last needed kachunk + tail elapsed)");
    }
}

void LEDManager::renderCooldownFade() {
    unsigned long elapsed = millis() - _stateStartMs;
    float t = (float)elapsed / (float)LED_BRIGHTNESS_FADE_MS;
    if (t > 1.0f) t = 1.0f;

    float brightness = _fireBrightness + (_idleMaxBrightness - _fireBrightness) * t;
    float ventBrightness = _ventFireBrightness + (_ventMaxBrightness - _ventFireBrightness) * t;

    renderBarrel(nullptr, brightness, brightness);
    renderVents(_colors[(uint8_t)ColorSlot::VENT], ventBrightness);

    if (t >= 1.0f) {
        _state = LEDState::IDLE;
        _stateStartMs = millis();
        Serial.println("[LEDManager] -> IDLE (breathing)");
    }
}
