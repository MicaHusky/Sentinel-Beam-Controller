#include "TriggerManager.h"
#include "Config.h"

void TriggerManager::begin() {
    pinMode(PIN_TRIGGER, INPUT_PULLUP);

    // Switch connects GPIO4 to GND when pressed -> LOW means pressed.
    _rawState     = (digitalRead(PIN_TRIGGER) == LOW);
    _lastRawState = _rawState;
    _stableState  = _rawState;
    _lastChangeMs = millis();

    _pressedEdge  = false;
    _releasedEdge = false;
}

void TriggerManager::update() {
    _pressedEdge  = false;
    _releasedEdge = false;

    // Switch connects GPIO4 to GND when pressed -> LOW means pressed.
    // OR'd with the web portal's virtual trigger override.
    _rawState = (digitalRead(PIN_TRIGGER) == LOW) || _webOverrideHeld;

    // Any raw change resets the debounce timer.
    if (_rawState != _lastRawState) {
        _lastChangeMs  = millis();
        _lastRawState  = _rawState;
    }

    // Only accept the new state once it has been stable for the debounce window.
    if ((millis() - _lastChangeMs) >= TRIGGER_DEBOUNCE_MS) {
        if (_rawState != _stableState) {
            _stableState = _rawState;
            if (_stableState) {
                _pressedEdge = true;
                Serial.println("[TriggerManager] PRESS edge");
            } else {
                _releasedEdge = true;
                Serial.println("[TriggerManager] RELEASE edge");
            }
        }
    }
}
