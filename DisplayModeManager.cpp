#include "DisplayModeManager.h"
#include "Config.h"

void DisplayModeManager::resetToDefaults() {
    activityMinSeconds    = DISPLAY_ACTIVITY_MIN_SEC;
    activityMaxSeconds    = DISPLAY_ACTIVITY_MAX_SEC;
    twitchCountMin        = DISPLAY_TWITCH_COUNT_MIN;
    twitchCountMax        = DISPLAY_TWITCH_COUNT_MAX;
    twitchMinSeconds      = DISPLAY_TWITCH_MIN_SEC;
    twitchMaxSeconds      = DISPLAY_TWITCH_MAX_SEC;
    twitchPauseMinSeconds = DISPLAY_TWITCH_PAUSE_MIN_SEC;
    twitchPauseMaxSeconds = DISPLAY_TWITCH_PAUSE_MAX_SEC;
    twitchRPM             = DISPLAY_TWITCH_RPM;
    twitchAccelPercent    = DISPLAY_TWITCH_ACCEL_PERCENT;
}

uint32_t DisplayModeManager::twitchSpeedHz() const {
    return (uint32_t)(twitchRPM * (float)MOTOR_STEPS_PER_BARREL_REV / 60.0f + 0.5f);
}

uint32_t DisplayModeManager::twitchAccelHz() const {
    uint32_t a = (uint32_t)((float)MOTOR_ACCEL_HZ_S2 * twitchAccelPercent / 100.0f + 0.5f);
    return a < 1 ? 1 : a;
}

uint32_t DisplayModeManager::randomMs(float aSec, float bSec) {
    float lo = aSec < bSec ? aSec : bSec;
    float hi = aSec < bSec ? bSec : aSec;
    uint32_t loMs = (uint32_t)(lo * 1000.0f);
    uint32_t hiMs = (uint32_t)(hi * 1000.0f);
    if (hiMs <= loMs) return loMs;
    return loMs + (esp_random() % (hiMs - loMs + 1)); // hardware RNG
}

void DisplayModeManager::scheduleNextActivity() {
    uint32_t waitMs = randomMs(activityMinSeconds, activityMaxSeconds);
    _phase = Phase::WAITING;
    _phaseUntilMs = millis() + waitMs;
    Serial.printf("[DisplayMode] next activity in %.1f s\n", waitMs / 1000.0f);
}

void DisplayModeManager::enter() {
    _active = true;
    Serial.printf("[DisplayMode] active - activity every %.0f-%.0f s, %u-%u twitches at %.1f RPM (%u Hz, accel %u Hz/s^2)\n",
                  activityMinSeconds, activityMaxSeconds, twitchCountMin, twitchCountMax,
                  twitchRPM, (unsigned)twitchSpeedHz(), (unsigned)twitchAccelHz());
    scheduleNextActivity();
}

void DisplayModeManager::exit() {
    if (_active) Serial.println("[DisplayMode] inactive");
    _active = false;
    _phase = Phase::WAITING;
}

DisplayEvent DisplayModeManager::update() {
    if (!_active) return DisplayEvent::NONE;
    const unsigned long now = millis();
    if ((long)(now - _phaseUntilMs) < 0) return DisplayEvent::NONE;

    switch (_phase) {
        case Phase::WAITING: {
            uint8_t lo = twitchCountMin < twitchCountMax ? twitchCountMin : twitchCountMax;
            uint8_t hi = twitchCountMin < twitchCountMax ? twitchCountMax : twitchCountMin;
            _twitchesLeft = lo + (uint8_t)(esp_random() % (uint32_t)(hi - lo + 1));
            Serial.printf("[DisplayMode] activity! %u twitch(es)\n", _twitchesLeft);
            // First twitch starts right away (next update).
            _phase = Phase::RESTING;
            _phaseUntilMs = now;
            return DisplayEvent::START_ACTIVITY;
        }
        case Phase::RESTING: {
            if (_twitchesLeft == 0) {
                // Burst done - barrel rests until activity.wav ends.
                _phase = Phase::ACTIVITY_TAIL;
                _phaseUntilMs = now + 0x7FFFFFFF; // effectively forever; notifyActivityEnded() moves on
                return DisplayEvent::NONE;
            }
            _twitchesLeft--;
            uint32_t runMs = randomMs(twitchMinSeconds, twitchMaxSeconds);
            bool forward = (esp_random() & 1) != 0;
            _phase = Phase::TWITCHING;
            _phaseUntilMs = now + runMs;
            Serial.printf("[DisplayMode] twitch %s for %.2f s\n", forward ? "forward" : "reverse", runMs / 1000.0f);
            return forward ? DisplayEvent::TWITCH_FORWARD : DisplayEvent::TWITCH_REVERSE;
        }
        case Phase::TWITCHING: {
            _phase = Phase::RESTING;
            _phaseUntilMs = now + randomMs(twitchPauseMinSeconds, twitchPauseMaxSeconds);
            return DisplayEvent::TWITCH_STOP;
        }
        case Phase::ACTIVITY_TAIL:
        default:
            return DisplayEvent::NONE;
    }
}

bool DisplayModeManager::notifyActivityEnded() {
    if (!_active) return false;
    bool wasTwitching = (_phase == Phase::TWITCHING);
    if (_phase != Phase::WAITING) {
        if (_twitchesLeft > 0 || wasTwitching) {
            Serial.println("[DisplayMode] activity.wav ended - cutting the twitch burst short");
        }
        _twitchesLeft = 0;
        scheduleNextActivity();
    }
    return wasTwitching;
}
