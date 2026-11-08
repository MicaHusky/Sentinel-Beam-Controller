#pragma once
#include <Arduino.h>
#include <FastAccelStepper.h>

// =============================================================================
// MotorManager
//
// Owns the barrel stepper. Reacts only to startFiring()/stopFiring() calls
// from the trigger, and startTwitch()/stopTwitch() from display mode - it has
// no idea whether audio is playing.
//
// EN handling: FastAccelStepper's auto-enable feature drives the TMC2209
// EN pin for us - energized while ramping/running, disabled the instant the
// motor is fully stopped. That gives the chosen behavior: a full ~0.5s
// controlled deceleration on release, then the coils de-energize so the
// idle barrel has zero holding torque.
// =============================================================================
class MotorManager {
public:
    bool begin(); // returns false if the stepper failed to initialize

    // Accelerate to firing speed. Safe to call whether the motor is
    // currently idle OR mid-deceleration from a just-released trigger -
    // FastAccelStepper will smoothly reverse the ramp back up.
    void startFiring();

    // Begin a controlled deceleration to zero. EN auto-disables once the
    // motor actually reaches standstill (see begin()).
    void stopFiring();

    // Display mode: slow move in either direction with its own speed/accel.
    // Same auto-enable/disable as firing - limp again once stopped.
    void startTwitch(bool forward, uint32_t speedHz, uint32_t accelHzPerS2);
    void stopTwitch(); // controlled decel at the twitch accel

    bool isRunning() const;

private:
    FastAccelStepperEngine _engine;
    FastAccelStepper*      _stepper = nullptr;
};
