#include "ModeManager.h"
#include "Config.h"

void ModeManager::begin() {
    pinMode(PIN_MODE_BUTTON, INPUT_PULLUP);

    // Switch connects the pin to GND when pressed -> LOW means pressed.
    _rawState     = (digitalRead(PIN_MODE_BUTTON) == LOW);
    _lastRawState = _rawState;
    _stableState  = _rawState;
    _lastChangeMs = millis();

    // If the button happens to be held at boot, don't treat it as a fresh long press.
    _longFired = _stableState;
    _tapCount  = 0;

    resetToDefaults();
    _current = PROFILE_STOCK;
    _hasPending = false;
    _requestJustMade = false;

    Serial.printf("[ModeManager] initialized on GPIO%u - profile: %s (long press %ums, multi-press gap %ums)\n",
                  (unsigned)PIN_MODE_BUTTON, profileName(_current),
                  (unsigned)_longPressMs, (unsigned)_multiPressGapMs);
}

void ModeManager::resetToDefaults() {
    _longPressMs     = MODE_LONG_PRESS_MS;
    _multiPressGapMs = MODE_MULTI_PRESS_GAP_MS;
}

void ModeManager::update() {
    const unsigned long now = millis();
    _rawState = (digitalRead(PIN_MODE_BUTTON) == LOW);

    // Any raw change resets the debounce timer.
    if (_rawState != _lastRawState) {
        _lastChangeMs = now;
        _lastRawState = _rawState;
    }

    // Only accept the new state once it has been stable for the debounce window.
    if ((now - _lastChangeMs) >= MODE_BUTTON_DEBOUNCE_MS) {
        if (_rawState != _stableState) {
            _stableState = _rawState;
            if (_stableState) {
                // press edge
                _pressStartMs = now;
                _longFired = false;
            } else {
                // release edge - a hold that already fired a long press doesn't count as a tap
                if (!_longFired) {
                    _tapCount++;
                    _lastReleaseMs = now;
                }
                _longFired = false;
            }
        }
    }

    // Long press: fires while still held, discarding any taps counted before it.
    if (_stableState && !_longFired && (now - _pressStartMs) >= _longPressMs) {
        _longFired = true;
        _tapCount = 0;
        Serial.println("[ModeManager] long press");
        requestProfile(PROFILE_STOCK, "long press");
    }

    // Taps: resolve once the button has stayed released for the multi-press gap.
    if (_tapCount > 0 && !_stableState && (now - _lastReleaseMs) >= _multiPressGapMs) {
        uint8_t taps = _tapCount;
        _tapCount = 0;
        if (taps <= 3) {
            Serial.printf("[ModeManager] %u-press\n", taps);
            requestProfile((ModeProfile)taps, taps == 1 ? "single press" : (taps == 2 ? "double press" : "triple press"));
        } else {
            Serial.printf("[ModeManager] %u presses - no gesture mapped, ignored\n", taps);
        }
    }
}

void ModeManager::requestProfile(ModeProfile p, const char* source) {
    if ((unsigned)p >= (unsigned)PROFILE_COUNT) return;
    if (_hasPending) {
        Serial.printf("[ModeManager] %s -> %s requested (replaces pending %s)\n",
                      source, profileName(p), profileName(_pending));
    } else {
        Serial.printf("[ModeManager] %s -> %s requested\n", source, profileName(p));
    }
    _pending = p;
    _hasPending = true;
    _requestJustMade = true;
}

ModeProfile ModeManager::takePending() {
    _hasPending = false;
    return _pending;
}

void ModeManager::markApplied(ModeProfile p) {
    _current = p;
    Serial.printf("[ModeManager] active profile: %s\n", profileName(_current));
}
