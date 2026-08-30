#pragma once
#include <Arduino.h>

// =============================================================================
// FogManager
//
// Foggers are small USB-powered piezo atomizer modules switched through a
// MOSFET on PIN_FOG. Straight digital on/off - no PWM, no ramping.
//
// Reacts only to startFiring()/stopFiring() from the main state machine:
// ON for the entire FIRING state, OFF at every other time (idle, cooldown).
// =============================================================================
class FogManager {
public:
    void begin();
    void startFiring(); // fogger ON
    void stopFiring();  // fogger OFF
};
