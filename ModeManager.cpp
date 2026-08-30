#include "ModeManager.h"
#include "Config.h"

void ModeManager::begin() {
    pinMode(PIN_MODE_BUTTON, INPUT_PULLUP);

    // Switch connects the pin to GND when pressed -> LOW means pressed.
    _rawState     = (digitalRead(PIN_MODE_BUTTON) == LOW);
    _lastRawState = _rawState;
    _stableState  = _rawState;
    _lastChangeMs = millis();

    _mode = MODE_NORMAL;

    Serial.printf("[ModeManager] initialized on GPIO%u - mode: %s\n",
                  (unsigned)PIN_MODE_BUTTON, modeName(_mode));
}

void ModeManager::update() {
    _rawState = (digitalRead(PIN_MODE_BUTTON) == LOW);

    // Any raw change resets the debounce timer.
    if (_rawState != _lastRawState) {
        _lastChangeMs = millis();
        _lastRawState = _rawState;
    }

    // Only accept the new state once it has been stable for the debounce window.
    if ((millis() - _lastChangeMs) >= MODE_BUTTON_DEBOUNCE_MS) {
        if (_rawState != _stableState) {
            _stableState = _rawState;
            if (_stableState) {
                advance(); // press edge only (button just pulled to GND)
            }
        }
    }
}

void ModeManager::advance() {
    _mode = (LEDMode)(((int)_mode + 1) % (int)MODE_COUNT);
    Serial.printf("[ModeManager] button press -> mode: %s\n", modeName(_mode));
}

void ModeManager::setMode(LEDMode m) {
    if ((unsigned)m >= (unsigned)MODE_COUNT) return; // also catches a negative value
    if (m == _mode) return;
    _mode = m;
    Serial.printf("[ModeManager] mode set to: %s\n", modeName(_mode));
}
