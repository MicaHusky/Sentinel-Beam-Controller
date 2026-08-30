#pragma once
#include <Arduino.h>
#include <FastAccelStepper.h>

// =============================================================================
// MotorManager
//
// Owns the barrel stepper. Reacts only to startFiring()/stopFiring() calls
// from the trigger - it has no idea whether audio is playing.
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

    bool isRunning() const;

private:
    FastAccelStepperEngine _engine;
    FastAccelStepper*      _stepper = nullptr;
};
