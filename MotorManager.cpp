#include "MotorManager.h"
#include "Config.h"

bool MotorManager::begin() {
    _engine.init();
    _stepper = _engine.stepperConnectToPin(PIN_STEPPER_STEP);

    if (!_stepper) {
        // stepperConnectToPin() failed - wrong pin for this MCU's RMT/step
        // generator, or all step generator slots already used.
        Serial.println("[MotorManager] stepperConnectToPin() FAILED - check PIN_STEPPER_STEP");
        return false;
    }

    _stepper->setDirectionPin(PIN_STEPPER_DIR);

    // TMC2209 EN is active LOW -> pass true for "low active".
    _stepper->setEnablePin(PIN_STEPPER_ENABLE, true);
    _stepper->setAutoEnable(true);
    _stepper->setDelayToDisable(0); // disable the instant motion fully stops
    _stepper->setDelayToEnable(50); // small settle time before stepping starts

    _stepper->setSpeedInHz(MOTOR_SPEED_HZ);
    _stepper->setAcceleration(MOTOR_ACCEL_HZ_S2);

    Serial.println("[MotorManager] stepper initialized OK");
    return true;
}

void MotorManager::startFiring() {
    if (!_stepper) return;
    Serial.println("[MotorManager] spin-up");

    // Cheap to reapply every time; keeps this call correct whether we're
    // starting from a dead stop or reversing an in-progress decel.
    _stepper->setSpeedInHz(MOTOR_SPEED_HZ);
    _stepper->setAcceleration(MOTOR_ACCEL_HZ_S2);
    _stepper->runForward();
}

void MotorManager::stopFiring() {
    if (!_stepper) return;
    Serial.println("[MotorManager] spin-down");
    _stepper->stopMove(); // decelerates using the current acceleration, then halts
}

bool MotorManager::isRunning() const {
    if (!_stepper) return false;
    return _stepper->isRunning();
}
