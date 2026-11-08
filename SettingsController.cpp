#include "SettingsController.h"

void SettingsController::begin(LEDManager* led, AudioManager* audio, ModeManager* mode, DisplayModeManager* display) {
    _led = led;
    _audio = audio;
    _mode = mode;
    _display = display;
}

bool SettingsController::parseHexColor(const String& hexIn, uint8_t& r, uint8_t& g, uint8_t& b) {
    String hex = hexIn;
    if (hex.startsWith("#")) hex = hex.substring(1);
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

float SettingsController::parsePercent(String value) {
    value.trim();
    if (value.endsWith("%")) value.remove(value.length() - 1);
    value.trim();
    if (!isNumber(value)) return -1.0f; // fails every 0-100 range check
    return value.toFloat();
}

// Strict-ish numeric check, so a typo in a mode.txt ("fast", "1.2.3") is
// reported instead of silently becoming 0 via toFloat().
bool SettingsController::isNumber(const String& value) {
    if (value.length() == 0) return false;
    bool digit = false, dot = false;
    for (unsigned i = 0; i < value.length(); i++) {
        char c = value[i];
        if (c >= '0' && c <= '9') { digit = true; continue; }
        if (c == '.' && !dot)     { dot = true; continue; }
        if ((c == '-' || c == '+') && i == 0) continue;
        return false;
    }
    return digit;
}

SettingResult SettingsController::applyColor(ColorSlot slot, const char* keyName, const String& value) {
    if (value.equalsIgnoreCase("rainbow")) {
        if (_led) _led->setColorRainbow(slot);
        return {true, String(keyName) + " has been changed to rainbow"};
    }
    uint8_t r, g, b;
    if (!parseHexColor(value, r, g, b)) {
        return {false, String("ERROR: ") + keyName + " expects 6 hex digits (e.g. " + keyName +
                       "=0055FF) or the word rainbow"};
    }
    if (_led) _led->setColor(slot, r, g, b);
    return {true, String(keyName) + " has been changed to " + value};
}

SettingResult SettingsController::apply(const String& keyIn, const String& valueIn, bool fromModeFile) {
    String key = keyIn;
    String value = valueIn;
    key.trim();
    value.trim();

    // Numeric keys: reject non-numbers up front, so a typo is reported rather
    // than silently becoming 0 via toFloat(). (Colors, kachunkTimestamps, and
    // mode have their own parsing below; percent keys go through parsePercent.)
    String lowerKey = key;
    lowerKey.toLowerCase();
    const bool numericKey =
        key.equalsIgnoreCase("fireWipeSeconds") || key.equalsIgnoreCase("kachunkTail") ||
        key.equalsIgnoreCase("idleBreatheSeconds") || key.equalsIgnoreCase("audioGain") ||
        key.equalsIgnoreCase("rainbowSpeed") || key.equalsIgnoreCase("longPressMs") ||
        key.equalsIgnoreCase("multiPressGapMs") || key.equalsIgnoreCase("displayExitHoldMs") ||
        lowerKey.startsWith("activity") || lowerKey.startsWith("twitch") ||
        (key.length() == 8 && key.substring(0, 7).equalsIgnoreCase("kachunk"));
    if (numericKey && !isNumber(value)) {
        return {false, "ERROR: " + key + " expects a number, got '" + value + "'."};
    }

    if (key.equalsIgnoreCase("idleColor")) {
        return applyColor(ColorSlot::IDLE, "idleColor", value);

    } else if (key.equalsIgnoreCase("fireColor")) {
        return applyColor(ColorSlot::FIRE, "fireColor", value);

    } else if (key.equalsIgnoreCase("ventColor")) {
        return applyColor(ColorSlot::VENT, "ventColor", value);

    } else if (key.equalsIgnoreCase("ventFireColor")) {
        return applyColor(ColorSlot::VENT_FIRE, "ventFireColor", value);

    } else if (key.equalsIgnoreCase("ventMaxBrightness")) {
        float percent = parsePercent(value);
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
            if (!isNumber(token)) {
                return {false, "ERROR: kachunkTimestamps has a non-numeric value '" + token + "'."};
            }
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
        float percent = parsePercent(value);
        if (percent < 0.0f || percent > 100.0f) {
            return {false, "ERROR: idleMinBrightness must be 0-100 (percent)."};
        }
        if (_led) _led->setIdleMinBrightness(percent / 100.0f);
        return {true, "Idle minimum brightness has been changed to " + String(percent, 0) + "%"};

    } else if (key.equalsIgnoreCase("idleMaxBrightness")) {
        float percent = parsePercent(value);
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

    } else if (key.equalsIgnoreCase("mode")) {
        // Routed through ModeManager exactly like the GPIO1 button: queued if
        // the beam is firing/cooling down, applied by the main loop once IDLE.
        if (fromModeFile) {
            return {false, "ERROR: mode= can't be used inside a mode.txt."};
        }
        ModeProfile p;
        if (value.equalsIgnoreCase("stock") || value == "0") p = PROFILE_STOCK;
        else if (value == "1" || value.equalsIgnoreCase("alt1") || value.equalsIgnoreCase("AltMode1")) p = PROFILE_ALT1;
        else if (value == "2" || value.equalsIgnoreCase("alt2") || value.equalsIgnoreCase("AltMode2")) p = PROFILE_ALT2;
        else if (value == "3" || value.equalsIgnoreCase("alt3") || value.equalsIgnoreCase("AltMode3")) p = PROFILE_ALT3;
        else if (value.equalsIgnoreCase("display") || value.equalsIgnoreCase("DisplayMode")) p = PROFILE_DISPLAY;
        else return {false, "ERROR: mode expects stock, 1, 2, 3, or display."};
        if (_mode) _mode->requestProfile(p, "console");
        return {true, String("Mode switch to ") + profileName(p) + " requested"};

    } else if (key.equalsIgnoreCase("fireBrightness") || key.equalsIgnoreCase("ventFireBrightness")) {
        float percent = parsePercent(value);
        if (percent < 0.0f || percent > 100.0f) {
            return {false, "ERROR: " + key + " must be 0-100 (percent)."};
        }
        if (_led) {
            if (key.equalsIgnoreCase("fireBrightness")) _led->setFireBrightness(percent / 100.0f);
            else                                        _led->setVentFireBrightness(percent / 100.0f);
        }
        return {true, key + " has been changed to " + String(percent, 0) + "%"};

    } else if (key.equalsIgnoreCase("rainbowBrightness")) {
        float percent = parsePercent(value);
        if (percent < 0.0f || percent > 100.0f) {
            return {false, "ERROR: rainbowBrightness must be 0-100 (percent)."};
        }
        if (_led) _led->setRainbowBrightness(percent / 100.0f);
        return {true, "Rainbow brightness has been changed to " + String(percent, 0) + "%"};

    } else if (key.equalsIgnoreCase("longPressMs")) {
        long ms = value.toInt();
        if (ms < 300 || ms > 5000) {
            return {false, "ERROR: longPressMs must be 300-5000."};
        }
        if (_mode) _mode->setLongPressMs((uint16_t)ms);
        return {true, "Mode button long-press time has been changed to " + String(ms) + " ms"};

    } else if (key.equalsIgnoreCase("displayExitHoldMs")) {
        long ms = value.toInt();
        if (ms < 2000 || ms > 15000) {
            return {false, "ERROR: displayExitHoldMs must be 2000-15000."};
        }
        if (_mode) _mode->setDisplayExitHoldMs((uint16_t)ms);
        return {true, "Display-mode exit hold has been changed to " + String(ms) + " ms"};

    } else if (key.equalsIgnoreCase("twitchCountMin") || key.equalsIgnoreCase("twitchCountMax")) {
        long n = value.toInt();
        if (n < 1 || n > 10 || value.indexOf('.') >= 0) {
            return {false, "ERROR: " + key + " must be a whole number 1-10."};
        }
        if (_display) {
            if (key.equalsIgnoreCase("twitchCountMin")) _display->twitchCountMin = (uint8_t)n;
            else                                         _display->twitchCountMax = (uint8_t)n;
        }
        return {true, key + " has been changed to " + String(n)};

    } else if (key.equalsIgnoreCase("activityMinSeconds") || key.equalsIgnoreCase("activityMaxSeconds") ||
               key.equalsIgnoreCase("twitchMinSeconds") || key.equalsIgnoreCase("twitchMaxSeconds") ||
               key.equalsIgnoreCase("twitchPauseMinSeconds") || key.equalsIgnoreCase("twitchPauseMaxSeconds") ||
               key.equalsIgnoreCase("twitchRPM") || key.equalsIgnoreCase("twitchAccelPercent")) {
        // Display-mode float settings: {name, min, max, target}. Each min/max
        // pair is used as min(a,b)..max(a,b), so no ordering rules here.
        struct Spec { const char* name; float lo, hi; float* target; };
        DisplayModeManager* d = _display;
        static DisplayModeManager dummy; // validation still runs if no display manager is wired
        if (!d) d = &dummy;
        const Spec specs[] = {
            {"activityMinSeconds",    1.0f,  3600.0f, &d->activityMinSeconds},
            {"activityMaxSeconds",    1.0f,  3600.0f, &d->activityMaxSeconds},
            {"twitchMinSeconds",      0.05f, 10.0f,   &d->twitchMinSeconds},
            {"twitchMaxSeconds",      0.05f, 10.0f,   &d->twitchMaxSeconds},
            {"twitchPauseMinSeconds", 0.0f,  10.0f,   &d->twitchPauseMinSeconds},
            {"twitchPauseMaxSeconds", 0.0f,  10.0f,   &d->twitchPauseMaxSeconds},
            {"twitchRPM",             0.5f,  DISPLAY_TWITCH_RPM_MAX, &d->twitchRPM},
            {"twitchAccelPercent",    5.0f,  200.0f,  &d->twitchAccelPercent},
        };
        float v = value.toFloat();
        for (const Spec& sp : specs) {
            if (!key.equalsIgnoreCase(sp.name)) continue;
            if (v < sp.lo || v > sp.hi) {
                return {false, String("ERROR: ") + sp.name + " must be " + String(sp.lo, 2) + "-" + String(sp.hi, 2) + "."};
            }
            *sp.target = v;
            return {true, String(sp.name) + " has been changed to " + String(v, 2) +
                          " (applies from the next display-mode activity)"};
        }
        return {false, "ERROR: unknown setting '" + key + "'."}; // unreachable

    } else if (key.equalsIgnoreCase("multiPressGapMs")) {
        long ms = value.toInt();
        if (ms < 100 || ms > 2000) {
            return {false, "ERROR: multiPressGapMs must be 100-2000."};
        }
        if (_mode) _mode->setMultiPressGapMs((uint16_t)ms);
        return {true, "Mode button multi-press gap has been changed to " + String(ms) + " ms"};

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
