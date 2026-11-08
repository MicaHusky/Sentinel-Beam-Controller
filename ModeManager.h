#pragma once
#include <Arduino.h>

// =============================================================================
// ModeManager
//
// Owns the second momentary button (PIN_MODE_BUTTON / GPIO1) and is the
// single source of truth for which mode profile is active and which one (if
// any) has been requested. Gestures:
//
//   single press -> PROFILE_ALT1    (/AltMode1 on the SD card)
//   double press -> PROFILE_ALT2    (/AltMode2)
//   triple press -> PROFILE_ALT3    (/AltMode3)
//   long press   -> PROFILE_STOCK   (root WAVs + Config.h defaults, as if just booted)
//
// Taps are counted until the button has been released for multiPressGapMs, so
// a single press acts that long after release. A long press acts the moment
// the hold crosses longPressMs (while still held) and discards any taps
// counted before it. 4+ taps are ignored.
//
// The debounce is a deliberate copy of TriggerManager's (keeps the hardware-
// validated trigger path untouched). Like every other manager it never calls
// other subsystems: the main sketch polls takeRequestJustMade() (to start the
// status-LED indicator immediately) and hasPending() (to actually load the
// profile once the beam is IDLE - presses during FIRING/COOLDOWN are queued,
// a newer request replacing an older one), then reports the outcome back with
// markApplied(). The debug console's `mode=` command goes through
// requestProfile() exactly like the button.
// =============================================================================

enum ModeProfile {
    PROFILE_STOCK = 0,
    PROFILE_ALT1  = 1,
    PROFILE_ALT2  = 2,
    PROFILE_ALT3  = 3,
    PROFILE_COUNT // keep last
};

inline const char* profileName(ModeProfile p) {
    switch (p) {
        case PROFILE_STOCK: return "Stock";
        case PROFILE_ALT1:  return "AltMode1";
        case PROFILE_ALT2:  return "AltMode2";
        case PROFILE_ALT3:  return "AltMode3";
        default:            return "?";
    }
}

class ModeManager {
public:
    void begin();
    void update(); // call once per loop() - debounces the button, detects gestures

    // ---- requests ----
    void requestProfile(ModeProfile p, const char* source); // button or console
    // True once per new request (button or console) - reading it clears it.
    bool takeRequestJustMade() { bool r = _requestJustMade; _requestJustMade = false; return r; }
    bool hasPending() const { return _hasPending; }
    ModeProfile pending() const { return _pending; }
    ModeProfile takePending();          // clears the pending request and returns it
    void markApplied(ModeProfile p);    // what actually ended up loaded (stock on a rejected alt mode)
    ModeProfile current() const { return _current; }

    // ---- runtime-tunable gesture timing (Config.h values are the boot defaults) ----
    void setLongPressMs(uint16_t ms)     { _longPressMs = ms; }
    void setMultiPressGapMs(uint16_t ms) { _multiPressGapMs = ms; }
    uint16_t getLongPressMs() const      { return _longPressMs; }
    uint16_t getMultiPressGapMs() const  { return _multiPressGapMs; }
    void resetToDefaults();

private:
    // ---- GPIO1 button debounce, same scheme as TriggerManager on GPIO4 ----
    bool _rawState      = false; // raw reading this update()
    bool _lastRawState  = false; // raw reading from the previous update()
    bool _stableState   = false; // debounced state (true = pressed)
    unsigned long _lastChangeMs = 0;

    // ---- gesture tracking ----
    unsigned long _pressStartMs  = 0;
    unsigned long _lastReleaseMs = 0;
    uint8_t _tapCount   = 0;
    bool    _longFired  = false; // the current hold already triggered a long press

    uint16_t _longPressMs     = 0;
    uint16_t _multiPressGapMs = 0;

    // ---- profile state ----
    ModeProfile _current = PROFILE_STOCK;
    ModeProfile _pending = PROFILE_STOCK;
    bool _hasPending      = false;
    bool _requestJustMade = false;
};
