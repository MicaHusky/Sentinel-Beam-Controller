#include "SettingsController.h"

void SettingsController::begin(LEDManager* led, AudioManager* audio, ModeManager* mode) {
    _led = led;
    _audio = audio;
    _mode = mode;
}

bool SettingsController::parseHexColor(const String& hex, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (hex.length() != 6) return false;

    for (uint8_t i = 0; i < 6; i++) {
        char c = hex[i];
        bool isHexDigit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!isHexDigit) return false;
    }

    long value = strtol(hex.c_str(), nullptr, 16);
    r = (uint8_t)((value >> 16) & 0xFF);
    g = (uint8_t)((value >> 8) & 0xFF);
    b = (uint8_t)(value & 0xFF);
    return true;
}

SettingResult SettingsController::apply(const String& keyIn, const String& valueIn) {
    String key = keyIn;
    String value = valueIn;
    key.trim();
    value.trim();

    if (key.equalsIgnoreCase("idleColor")) {
        uint8_t r, g, b;
        if (!parseHexColor(value, r, g, b)) {
            return {false, "ERROR: idleColor expects 6 hex digits, e.g. idleColor=33B5E5"};
        }
        if (_led) _led->setIdleColor(r, g, b);
        return {true, "Idle color has been changed to " + value};

    } else if (key.equalsIgnoreCase("fireColor")) {
        uint8_t r, g, b;
        if (!parseHexColor(value, r, g, b)) {
            return {false, "ERROR: fireColor expects 6 hex digits, e.g. fireColor=FF6400"};
        }
        if (_led) _led->setFireColor(r, g, b);
        return {true, "Fire color has been changed to " + value};

    } else if (key.equalsIgnoreCase("ventColor")) {
        uint8_t r, g, b;
        if (!parseHexColor(value, r, g, b)) {
            return {false, "ERROR: ventColor expects 6 hex digits, e.g. ventColor=E8A020"};
        }
        if (_led) _led->setVentColor(r, g, b);
        return {true, "Vent color has been changed to " + value};

    } else if (key.equalsIgnoreCase("ventMaxBrightness")) {
        float percent = value.toFloat();
        if (percent < 0.0f || percent > 100.0f) {
            return {false, "ERROR: ventMaxBrightness must be 0-100 (percent)."};
        }
        if (_led) _led->setVentMaxBrightness(percent / 100.0f);
        return {true, "Vent maximum brightness has been changed to " + String(percent, 0) + "%"};

    } else if (key.equalsIgnoreCase("fireWipeSeconds")) {
        float seconds = value.toFloat();
        if (seconds <= 0.0f || seconds > 60.0f) {
            return {false, "ERROR: fireWipeSeconds must be > 0 and <= 60."};
        }
        if (_led) _led->setFireWipeSeconds(seconds);
        return {true, "Fire wipe duration has been changed to " + String(seconds, 2) + " seconds"};

    } else if (key.equalsIgnoreCase("kachunkTimestamps")) {
        float parsed[5];
        uint8_t count = 0;
        int start = 0;
        while (count < 5) {
            int comma = value.indexOf(',', start);
            String token = (comma < 0) ? value.substring(start) : value.substring(start, comma);
            token.trim();
            if (token.length() == 0) break;
            parsed[count] = token.toFloat();
            count++;
            if (comma < 0) break;
            start = comma + 1;
        }
        if (count != 5) {
            return {false, "ERROR: kachunkTimestamps expects exactly 5 comma-separated values, e.g. kachunkTimestamps=0.260,0.680,1.080,1.490,1.964"};
        }
        for (uint8_t i = 0; i < 5; i++) {
            if (parsed[i] < 0.0f || parsed[i] > 60.0f) {
                return {false, "ERROR: each kachunk timestamp must be 0-60 seconds."};
            }
            if (i > 0 && parsed[i] <= parsed[i - 1]) {
                return {false, "ERROR: kachunk timestamps must be strictly ascending."};
            }
        }
        if (_led) _led->setKachunkTimestamps(parsed[0], parsed[1], parsed[2], parsed[3], parsed[4]);
        return {true, "Kachunk timestamps have been changed to " + String(parsed[0], 3) + ", " + String(parsed[1], 3) +
                       ", " + String(parsed[2], 3) + ", " + String(parsed[3], 3) + ", " + String(parsed[4], 3) + " sec"};

    } else if (key.equalsIgnoreCase("kachunk1") || key.equalsIgnoreCase("kachunk2") ||
               key.equalsIgnoreCase("kachunk3") || key.equalsIgnoreCase("kachunk4") ||
               key.equalsIgnoreCase("kachunk5")) {
        uint8_t idx = key.charAt(key.length() - 1) - '1'; // 'kachunk3' -> index 2
        float seconds = value.toFloat();
        if (seconds < 0.0f || seconds > 60.0f) {
            return {false, "ERROR: kachunk timestamp must be 0-60 seconds."};
        }
        if (_led) {
            bool ok = true;
            if (idx > 0 && seconds <= _led->getKachunkTimestamp(idx - 1)) ok = false;
            if (idx < 4 && seconds >= _led->getKachunkTimestamp(idx + 1)) ok = false;
            if (!ok) {
                return {false, "ERROR: kachunk timestamps must stay in ascending order (each later than the previous, earlier than the next)."};
            }
            _led->setKachunkTimestamp(idx, seconds);
        }
        return {true, "Kachunk " + String(idx + 1) + " timestamp has been changed to " + String(seconds, 3) + " sec"};

    } else if (key.equalsIgnoreCase("kachunkTail")) {
        float tail = value.toFloat();
        if (tail < 0.0f || tail > 10.0f) {
            return {false, "ERROR: kachunkTail must be 0-10 seconds."};
        }
        if (_led) _led->setKachunkTailSeconds(tail);
        return {true, "Kachunk tail time has been changed to " + String(tail, 3) + " sec"};

    } else if (key.equalsIgnoreCase("idleBreatheSeconds")) {
        float seconds = value.toFloat();
        if (seconds <= 0.0f || seconds > 60.0f) {
            return {false, "ERROR: idleBreatheSeconds must be > 0 and <= 60."};
        }
        if (_led) _led->setIdleBreatheSeconds(seconds);
        return {true, "Idle breathe period has been changed to " + String(seconds, 2) + " seconds"};

    } else if (key.equalsIgnoreCase("idleMinBrightness")) {
        float percent = value.toFloat();
        if (percent < 0.0f || percent > 100.0f) {
            return {false, "ERROR: idleMinBrightness must be 0-100 (percent)."};
        }
        if (_led) _led->setIdleMinBrightness(percent / 100.0f);
        return {true, "Idle minimum brightness has been changed to " + String(percent, 0) + "%"};

    } else if (key.equalsIgnoreCase("idleMaxBrightness")) {
        float percent = value.toFloat();
        if (percent < 0.0f || percent > 100.0f) {
            return {false, "ERROR: idleMaxBrightness must be 0-100 (percent)."};
        }
        if (_led) _led->setIdleMaxBrightness(percent / 100.0f);
        return {true, "Idle maximum brightness has been changed to " + String(percent, 0) + "%"};

    } else if (key.equalsIgnoreCase("audioGain")) {
        float gain = value.toFloat();
        if (gain < 0.0f || gain > 2.0f) {
            return {false, "ERROR: audioGain must be 0.0-2.0 (1.0 = unity gain)."};
        }
        if (_audio) _audio->setGain(gain);
        return {true, "Audio gain has been changed to " + String(gain, 2)};

    } else if (key.equalsIgnoreCase("ledMode")) {
        // Routed through ModeManager (the single source of truth), so this and
        // the physical GPIO1 mode button stay in agreement. The main loop
        // applies the change to the LEDs on its next pass.
        if (value.equalsIgnoreCase("rainbow")) {
            if (_mode) _mode->setMode(MODE_RAINBOW);
            return {true, "LED mode has been changed to rainbow"};
        } else if (value.equalsIgnoreCase("normal")) {
            if (_mode) _mode->setMode(MODE_NORMAL);
            return {true, "LED mode has been changed to normal"};
        }
        return {false, "ERROR: ledMode expects 'normal' or 'rainbow'."};

    } else if (key.equalsIgnoreCase("rainbowSpeed")) {
        float speed = value.toFloat();
        if (speed <= 0.0f || speed > 500.0f) {
            return {false, "ERROR: rainbowSpeed must be > 0 and <= 500."};
        }
        if (_led) _led->setRainbowSpeed(speed);
        return {true, "Rainbow speed has been changed to " + String(speed, 1)};
    }

    return {false, "ERROR: unknown setting '" + key + "'."};
}
