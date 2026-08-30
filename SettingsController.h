#pragma once
#include <Arduino.h>
#include "LEDManager.h"
#include "AudioManager.h"
#include "ModeManager.h"

// =============================================================================
// SettingsController
//
// The single place that knows how to validate and apply a "key=value"
// setting. DebugConsole (Serial) and WebPortal (phone web page) both call
// apply() instead of each having their own copy of this logic - one set of
// validation rules, two front ends.
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
    void begin(LEDManager* led, AudioManager* audio, ModeManager* mode);
    SettingResult apply(const String& key, const String& value);

private:
    static bool parseHexColor(const String& hex, uint8_t& r, uint8_t& g, uint8_t& b);

    LEDManager*   _led   = nullptr;
    AudioManager* _audio = nullptr;
    ModeManager*  _mode  = nullptr;
};
