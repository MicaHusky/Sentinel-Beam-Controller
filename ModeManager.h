#pragma once
#include <Arduino.h>

// =============================================================================
// ModeManager
//
// Owns the second momentary button (PIN_MODE_BUTTON / GPIO1) and the current
// LED mode. The button is debounced with the exact same scheme TriggerManager
// uses on GPIO4; each press advances one step through LEDMode and wraps around.
//
// This is the single source of truth for "what mode are we in" - both the
// physical button and the debug console's `ledMode=` command go through here,
// so they can never disagree. Like every other manager it does NOT touch other
// subsystems directly: the main sketch polls mode() each loop and, whenever it
// changes, translates the new mode into the appropriate subsystem calls (see
// applyMode() in SentinelBeam.ino).
//
// Adding a new mode = three edits: a value here in LEDMode (before MODE_COUNT),
// a case in modeName() below, and a case in applyMode() in the main sketch.
// =============================================================================

enum LEDMode {
    MODE_NORMAL = 0, // normal idle / firing / cooldown rendering
    MODE_RAINBOW,    // full-strip rainbow chase override (barrel + vents)
    MODE_COUNT       // keep last - number of modes, used for cycling
};

inline const char* modeName(LEDMode m) {
    switch (m) {
        case MODE_NORMAL:  return "Normal";
        case MODE_RAINBOW: return "Rainbow";
        default:           return "?";
    }
}

class ModeManager {
public:
    void begin();
    void update(); // call once per loop() - debounces the button, advances mode on a press

    LEDMode mode() const { return _mode; }

    // Used by the debug console (`ledMode=normal|rainbow`). Out-of-range values
    // are ignored. The main sketch applies the change on its next loop, exactly
    // as it would a button press.
    void setMode(LEDMode m);

private:
    void advance(); // -> next mode in the cycle, with a Serial log

    // ---- GPIO1 button debounce, same scheme as TriggerManager on GPIO4 ----
    bool _rawState      = false; // raw reading this update()
    bool _lastRawState  = false; // raw reading from the previous update()
    bool _stableState   = false; // debounced state (true = pressed)
    unsigned long _lastChangeMs = 0;

    LEDMode _mode = MODE_NORMAL;
};
