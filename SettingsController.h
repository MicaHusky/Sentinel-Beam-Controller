#pragma once
#include <Arduino.h>
#include "LEDManager.h"
#include "AudioManager.h"
#include "ModeManager.h"
#include "DisplayModeManager.h"

// =============================================================================
// SettingsController
//
// The single place that knows how to validate and apply a "key=value"
// setting. DebugConsole (Serial) and each alt mode's mode.txt (fed in line by
// line by the main sketch) both call apply() instead of each having their own
// copy of this logic - one set of validation rules, two front ends, so any
// key the console accepts is also valid in a mode.txt and vice versa
// (except `mode=`, which a mode file isn't allowed to use).
//
// Colors accept 6 hex digits (optional leading '#') or the word "rainbow".
//
// message text does NOT include a "[Debug]" prefix or JSON wrapping - each
// caller formats the result however suits its own output.
// =============================================================================

struct SettingResult {
    bool ok;
    String message;
};

class SettingsController {
public:
    void begin(LEDManager* led, AudioManager* audio, ModeManager* mode, DisplayModeManager* display);
    // fromModeFile: the line came from an alt mode's mode.txt (rejects `mode=`).
    SettingResult apply(const String& key, const String& value, bool fromModeFile = false);

private:
    static bool parseHexColor(const String& hex, uint8_t& r, uint8_t& g, uint8_t& b);
    SettingResult applyColor(ColorSlot slot, const char* keyName, const String& value);
    static float parsePercent(String value); // tolerates a trailing '%'
    static bool isNumber(const String& value);

    LEDManager*   _led   = nullptr;
    AudioManager* _audio = nullptr;
    ModeManager*  _mode  = nullptr;
    DisplayModeManager* _display = nullptr;
};
