// =============================================================================
// Sentinel Beam - main sketch
//
// Board:   ESP32S3 Dev Module
// Flash:   16 MB
// PSRAM:   OPI PSRAM (must be enabled in board settings)
//
// Boot:    load idle.wav / fire.wav / cooldown.wav into PSRAM, then start
//          looping idle.wav once every subsystem has checked out OK.
// Hold:    stop idle.wav, start fire.wav, spin the motor up.
// Release  fire.wav stops instantly, motor begins its controlled spin-down,
// (or EOF): cooldown.wav starts. LED rings hard-switch back to idle color one
//          by one, cued by manually-tuned kachunk timestamps - only as many
//          kachunks as rings actually lit are used, so an early release uses
//          fewer beats and finishes sooner. Once the last needed kachunk +
//          tail time has elapsed, idle.wav resumes looping. A new trigger
//          press at any point (including mid-cooldown) immediately cuts back
//          into a fresh firing cycle.
// =============================================================================

#include "Config.h"
#include "TriggerManager.h"
#include "MotorManager.h"
#include "AudioManager.h"
#include "LEDManager.h"
#include "FogManager.h"
#include "SettingsController.h"
#include "DebugConsole.h"

enum SystemState {
    STATE_INIT,
    STATE_IDLE,
    STATE_FIRING,
    STATE_COOLDOWN
};

TriggerManager triggerManager;
MotorManager   motorManager;
AudioManager   audioManager;
LEDManager     ledManager;
FogManager     fogManager;
SettingsController settingsController;
DebugConsole   debugConsole;

SystemState currentState = STATE_INIT;
bool systemReady = false;

void setup() {
    Serial.begin(115200);
    delay(200); // give native USB CDC time to enumerate before first prints
    Serial.println();
    Serial.printf("[System] Sentinel Beam firmware v%s booting...\n", FIRMWARE_VERSION);

    triggerManager.begin();
    ledManager.begin();
    fogManager.begin();

    bool motorOk = motorManager.begin();
    bool audioOk = audioManager.begin();

    settingsController.begin(&ledManager, &audioManager);
    debugConsole.begin(&ledManager, &audioManager, &settingsController);

    if (motorOk && audioOk) {
        systemReady = true;
        currentState = STATE_IDLE;
        audioManager.playIdleLoop();
        Serial.println("[System] All checks passed - idle loop started, ready to fire.");
    } else {
        systemReady = false;
        Serial.print("[System] INIT FAILED (motor=");
        Serial.print(motorOk ? "OK" : "FAILED");
        Serial.print(", audio=");
        Serial.print(audioOk ? "OK" : "FAILED");
        Serial.println(") - trigger input disabled, staying inert.");
    }
}

void updateTrigger() { triggerManager.update(); }
void updateAudio()   { audioManager.update(); }
void updateMotor()   {
    // FastAccelStepper runs its ramps and auto-enable/disable in the
    // background; nothing to poll here yet. Placeholder kept so the loop
    // stays symmetric as more managers (rumble, servos) are added.
}
void updateLEDs()    { ledManager.update(); }
// FogManager is edge-driven only (startFiring()/stopFiring()) - no update() needed.

// Future: updateRumble()

void loop() {
    debugConsole.update(); // always available, even if boot failed

    if (!systemReady) {
        return; // see the INIT FAILED message printed once in setup()
    }

    updateTrigger();
    updateAudio();
    updateMotor();
    updateLEDs();

    if (triggerManager.wasPressed()) {
        Serial.println("[System] IDLE/COOLDOWN -> FIRING");
        audioManager.playFire();
        motorManager.startFiring();
        ledManager.notifyFireStart();
        fogManager.startFiring();
        currentState = STATE_FIRING;
    }

    if (currentState == STATE_FIRING && triggerManager.wasReleased()) {
        Serial.println("[System] FIRING -> COOLDOWN (trigger released)");
        audioManager.playCooldown();
        motorManager.stopFiring();
        ledManager.notifyFireEnd();
        fogManager.stopFiring();
        currentState = STATE_COOLDOWN;
    }

    if (currentState == STATE_FIRING && audioManager.fireReachedEOF()) {
        Serial.println("[System] FIRING -> COOLDOWN (fire.wav ended, trigger still held)");
        audioManager.playCooldown();
        motorManager.stopFiring();
        ledManager.notifyFireEnd();
        fogManager.stopFiring();
        currentState = STATE_COOLDOWN;
    }

    if (currentState == STATE_COOLDOWN && (ledManager.cooldownWipeComplete() || audioManager.cooldownReachedEOF())) {
        Serial.println("[System] COOLDOWN -> IDLE");
        audioManager.playIdleLoop();
        currentState = STATE_IDLE;
    }
}
