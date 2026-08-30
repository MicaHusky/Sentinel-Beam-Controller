#include "LEDManager.h"
#include "Config.h"

LEDManager::LEDManager()
    : _strip(LED_TOTAL_COUNT, PIN_LED_DATA, NEO_GRB + NEO_KHZ800),
      _ventLeft(VENT_LED_COUNT, PIN_VENT_LEFT, NEO_GRB + NEO_KHZ800),
      _ventRight(VENT_LED_COUNT, PIN_VENT_RIGHT, NEO_GRB + NEO_KHZ800),
      _idleR(LED_COLOR_IDLE_DEFAULT_R), _idleG(LED_COLOR_IDLE_DEFAULT_G), _idleB(LED_COLOR_IDLE_DEFAULT_B),
      _fireR(LED_COLOR_FIRE_DEFAULT_R), _fireG(LED_COLOR_FIRE_DEFAULT_G), _fireB(LED_COLOR_FIRE_DEFAULT_B),
      _fireWipeSeconds(LED_FIRE_WIPE_SECONDS),
      _idleBreathePeriodMs(LED_IDLE_BREATHE_PERIOD_MS),
      _idleMinBrightness(LED_IDLE_MIN_BRIGHTNESS),
      _idleMaxBrightness(LED_IDLE_MAX_BRIGHTNESS),
      _kachunkTimestampSec{LED_KACHUNK_1_SEC, LED_KACHUNK_2_SEC, LED_KACHUNK_3_SEC, LED_KACHUNK_4_SEC, LED_KACHUNK_5_SEC},
      _kachunkTailSec(LED_KACHUNK_TAIL_SEC),
      _rainbowSpeed(LED_RAINBOW_SPEED_DEFAULT),
      _ventR(LED_COLOR_VENT_DEFAULT_R), _ventG(LED_COLOR_VENT_DEFAULT_G), _ventB(LED_COLOR_VENT_DEFAULT_B),
      _ventMaxBrightness(LED_VENT_MAX_BRIGHTNESS) {
}

void LEDManager::begin() {
    _strip.begin();
    _strip.clear(); // LEDs beyond LED_ACTIVE_COUNT are never touched again - they stay off
    _strip.show();

    _ventLeft.begin();
    _ventLeft.clear();
    _ventLeft.show();

    _ventRight.begin();
    _ventRight.clear();
    _ventRight.show();

    for (uint8_t i = 0; i < LED_NUM_SETS; i++) {
        _setMix[i] = 0.0f;
    }

    _state = LEDState::IDLE;
    _stateStartMs = millis();

    Serial.printf("[LEDManager] initialized: %u sets of %u LEDs (%u active of %u total) + 2 vent strips of %u\n",
                  LED_NUM_SETS, LED_SET_SIZE, LED_ACTIVE_COUNT, LED_TOTAL_COUNT, VENT_LED_COUNT);
}

void LEDManager::notifyFireStart() {
    _state = LEDState::FIRING;
    _stateStartMs = millis();
    Serial.println("[LEDManager] -> FIRING (idle-color->fire-color wipe begins)");
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
    _ventLeft.clear();  _ventLeft.show();   // vents stay dark for the whole boot sequence
    _ventRight.clear(); _ventRight.show();

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
    const char* label;
    switch (result) {
        case BootResult::OK:
            r = LED_COLOR_BOOT_OK_R;   g = LED_COLOR_BOOT_OK_G;   b = LED_COLOR_BOOT_OK_B;
            count = 1; label = "GREEN x1 (all checks passed)";
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
        for (uint16_t i = 0; i < LED_ACTIVE_COUNT; i++) _strip.setPixelColor(i, on);
        _strip.show();
        delay(LED_BOOT_FLASH_ON_MS);
        _strip.clear();
        _strip.show();
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
    Serial.println("[LEDManager] -> IDLE (breathing)");
}

void LEDManager::setIdleColor(uint8_t r, uint8_t g, uint8_t b) {
    _idleR = r; _idleG = g; _idleB = b;
}

void LEDManager::setFireColor(uint8_t r, uint8_t g, uint8_t b) {
    _fireR = r; _fireG = g; _fireB = b;
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

void LEDManager::setVentColor(uint8_t r, uint8_t g, uint8_t b) {
    _ventR = r; _ventG = g; _ventB = b;
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

void LEDManager::setRainbowMode(bool enabled) {
    _rainbowMode = enabled;
    if (enabled) {
        _rainbowStartMs = millis(); // fresh reference point, avoids any discontinuity on toggle
    }
}

void LEDManager::setRainbowSpeed(float wheelPositionsPerSecond) {
    _rainbowSpeed = wheelPositionsPerSecond;
}

void LEDManager::update() {
    _cooldownWipeJustCompleted = false; // one-shot flag - reset every call, set true only the frame it fires

    if (_rainbowMode) {
        renderRainbow();
    } else {
        switch (_state) {
            case LEDState::BOOT:           renderBoot();          break;
            case LEDState::IDLE:           renderIdle();          break;
            case LEDState::FIRING:         renderFiring();        break;
            case LEDState::COOLDOWN_WIPE:  renderCooldownWipe();  break;
            case LEDState::COOLDOWN_FADE:  renderCooldownFade();  break;
        }
    }
    _strip.show();
    _ventLeft.show();
    _ventRight.show();
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

void LEDManager::setSetColor(uint8_t setIndex, uint8_t r, uint8_t g, uint8_t b, float brightness) {
    uint8_t rr = (uint8_t)((float)r * brightness);
    uint8_t gg = (uint8_t)((float)g * brightness);
    uint8_t bb = (uint8_t)((float)b * brightness);

    uint16_t startLed = (uint16_t)setIndex * LED_SET_SIZE;
    for (uint16_t i = 0; i < LED_SET_SIZE; i++) {
        _strip.setPixelColor(startLed + i, _strip.Color(rr, gg, bb));
    }
}

void LEDManager::setVentStrips(uint8_t r, uint8_t g, uint8_t b, float brightness) {
    uint8_t rr = (uint8_t)((float)r * brightness);
    uint8_t gg = (uint8_t)((float)g * brightness);
    uint8_t bb = (uint8_t)((float)b * brightness);

    uint32_t leftColor = _ventLeft.Color(rr, gg, bb);
    uint32_t rightColor = _ventRight.Color(rr, gg, bb);
    for (uint16_t i = 0; i < VENT_LED_COUNT; i++) {
        _ventLeft.setPixelColor(i, leftColor);
        _ventRight.setPixelColor(i, rightColor);
    }
}

void LEDManager::renderIdle() {
    unsigned long elapsed = (millis() - _stateStartMs) % _idleBreathePeriodMs;
    float phase = (2.0f * PI * (float)elapsed) / (float)_idleBreathePeriodMs;
    float breathe = (sinf(phase) * 0.5f) + 0.5f; // 0..1

    float brightness = _idleMinBrightness + (_idleMaxBrightness - _idleMinBrightness) * breathe;
    float ventBrightness = _idleMinBrightness + (_ventMaxBrightness - _idleMinBrightness) * breathe;

    for (uint8_t i = 0; i < LED_NUM_SETS; i++) {
        setSetColor(i, _idleR, _idleG, _idleB, brightness);
    }
    setVentStrips(_ventR, _ventG, _ventB, ventBrightness);
}

void LEDManager::renderFiring() {
    unsigned long elapsed = millis() - _stateStartMs;
    float overallProgress = (float)elapsed / (_fireWipeSeconds * 1000.0f);
    if (overallProgress > 1.0f) overallProgress = 1.0f;

    for (uint8_t i = 0; i < LED_NUM_SETS; i++) {
        float segP = segmentProgress(i, LED_NUM_SETS, overallProgress); // set 0 wipes first
        _setMix[i] = segP;

        uint8_t r = lerp8(_idleR, _fireR, segP);
        uint8_t g = lerp8(_idleG, _fireG, segP);
        uint8_t b = lerp8(_idleB, _fireB, segP);
        setSetColor(i, r, g, b, LED_FIRE_BRIGHTNESS);
    }
    setVentStrips(_ventR, _ventG, _ventB, LED_FIRE_BRIGHTNESS);
}

void LEDManager::renderCooldownWipe() {
    if (_neededBeats == 0) {
        // Nothing lit up before release - nothing to revert, skip straight to the fade.
        _state = LEDState::COOLDOWN_FADE;
        _stateStartMs = millis();
        _cooldownWipeJustCompleted = true;
        Serial.println("[LEDManager] -> COOLDOWN_FADE (no rings were lit, skipping wipe)");
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

    for (uint8_t i = 0; i < LED_NUM_SETS; i++) {
        uint8_t r = lerp8(_idleR, _fireR, _setMix[i]);
        uint8_t g = lerp8(_idleG, _fireG, _setMix[i]);
        uint8_t b = lerp8(_idleB, _fireB, _setMix[i]);
        setSetColor(i, r, g, b, LED_FIRE_BRIGHTNESS);
    }
    setVentStrips(_ventR, _ventG, _ventB, LED_FIRE_BRIGHTNESS);

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

    float brightness = LED_FIRE_BRIGHTNESS + (_idleMaxBrightness - LED_FIRE_BRIGHTNESS) * t;
    float ventBrightness = LED_FIRE_BRIGHTNESS + (_ventMaxBrightness - LED_FIRE_BRIGHTNESS) * t;

    for (uint8_t i = 0; i < LED_NUM_SETS; i++) {
        setSetColor(i, _idleR, _idleG, _idleB, brightness);
    }
    setVentStrips(_ventR, _ventG, _ventB, ventBrightness);

    if (t >= 1.0f) {
        _state = LEDState::IDLE;
        _stateStartMs = millis();
        Serial.println("[LEDManager] -> IDLE (breathing)");
    }
}

uint32_t LEDManager::colorWheel(uint8_t wheelPos) const {
    // Classic 0-255 RGB color wheel (red -> green -> blue -> red).
    wheelPos = 255 - wheelPos;
    if (wheelPos < 85) {
        return _strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
    } else if (wheelPos < 170) {
        wheelPos -= 85;
        return _strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
    } else {
        wheelPos -= 170;
        return _strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
    }
}

void LEDManager::renderRainbow() {
    unsigned long elapsed = millis() - _rainbowStartMs;
    float baseWheelPos = fmodf(((float)elapsed / 1000.0f) * _rainbowSpeed, 256.0f);

    for (uint16_t i = 0; i < LED_ACTIVE_COUNT; i++) {
        float pos = fmodf(baseWheelPos + ((float)i * 256.0f / (float)LED_ACTIVE_COUNT), 256.0f);
        _strip.setPixelColor(i, colorWheel((uint8_t)pos));
    }

    for (uint16_t i = 0; i < VENT_LED_COUNT; i++) {
        float pos = fmodf(baseWheelPos + ((float)i * 256.0f / (float)VENT_LED_COUNT), 256.0f);
        uint32_t c = colorWheel((uint8_t)pos);
        _ventLeft.setPixelColor(i, c);
        _ventRight.setPixelColor(i, c);
    }
}
