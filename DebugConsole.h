#pragma once
#include <Arduino.h>
#include "LEDManager.h"
#include "AudioManager.h"
#include "SettingsController.h"
#include "ModeManager.h"

// =============================================================================
// DebugConsole
//
// Non-blocking Serial command line: type `key=value` in the Arduino IDE's
// Serial Monitor and hit enter to live-tweak a setting until next reboot.
// Type `help` for the full command list.
//
// All the actual key/value validation and applying lives in
// SettingsController - this class only handles reading Serial lines and
// formatting the response, plus the `help` listing (Serial-only, since the
// web page's own layout serves as its "help").
//
// All changes here are RUNTIME ONLY - nothing is written back to the SD
// card. They're gone on the next power cycle / reset, exactly like any
// other in-memory variable.
// =============================================================================
class DebugConsole {
public:
    void begin(LEDManager* led, AudioManager* audio, SettingsController* settings, ModeManager* mode);
    void update(); // call once per loop() - reads whatever's arrived on Serial, non-blocking

private:
    void processLine(String line);
    void printHelp();
    static String colorToHex(uint8_t r, uint8_t g, uint8_t b);

    LEDManager*         _led      = nullptr;
    AudioManager*       _audio    = nullptr;
    SettingsController* _settings = nullptr;
    ModeManager*        _mode     = nullptr;
    String _lineBuffer;
};
