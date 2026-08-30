#pragma once
#include <Arduino.h>

// =============================================================================
// TriggerManager
//
// The trigger is the ONLY master input for the whole system. This class does
// nothing but debounce GPIO4 and expose clean edges:
//   - isPressed()   : current stable state
//   - wasPressed()  : true for exactly one update() call, on the press edge
//   - wasReleased() : true for exactly one update() call, on the release edge
//
// setWebOverride() lets the phone web page's "hold to fire" button act as a
// second physical trigger: it's logically OR'd with the real GPIO reading
// before debouncing, so a web-held trigger behaves identically to a
// hardware-held one (including needing an actual release before a new press
// registers, per the normal debounce/edge logic below).
//
// Nothing here knows about audio or the motor - it only reports trigger state.
// =============================================================================
class TriggerManager {
public:
    void begin();
    void update(); // call once per loop(), before reading edges

    bool isPressed()   const { return _stableState; }
    bool wasPressed()  const { return _pressedEdge; }
    bool wasReleased() const { return _releasedEdge; }

    void setWebOverride(bool held) { _webOverrideHeld = held; }

private:
    bool _rawState      = false; // raw reading this update()
    bool _lastRawState   = false; // raw reading from the previous update()
    bool _stableState    = false; // debounced state
    unsigned long _lastChangeMs = 0;

    bool _pressedEdge  = false;
    bool _releasedEdge = false;

    bool _webOverrideHeld = false; // set by WebPortal's /trigger endpoint
};
