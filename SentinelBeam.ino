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
#include "ModeManager.h"
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

// The boot progress bar maps one barrel ring to each boot phase, so it assumes
// exactly five rings (inits / idle.wav / fire.wav / cooldown.wav / after).
static_assert(LED_NUM_SETS == 5,
              "boot progress bar assumes exactly 5 barrel rings");

TriggerManager triggerManager;
ModeManager    modeManager;
MotorManager   motorManager;
AudioManager   audioManager;
LEDManager     ledManager;
FogManager     fogManager;
SettingsController settingsController;
DebugConsole   debugConsole;

SystemState currentState = STATE_INIT;
bool systemReady = false;
bool motorInitOk = false; // latched from motorManager.begin(); gates boot ring 0's SD-mount step
LEDMode appliedMode = MODE_NORMAL; // last mode actually pushed to the subsystems

// Translate the current mode into subsystem calls. The single place modes are
// acted on - add new modes here (and in LEDMode / modeName() in ModeManager.h).
// Managers still never call each other; this runs from the main loop.
void applyMode(LEDMode m) {
    switch (m) {
        case MODE_RAINBOW:
            ledManager.setRainbowMode(true);
            break;
        case MODE_NORMAL:
        default:
            ledManager.setRainbowMode(false);
            break;
    }
    Serial.printf("[System] LED mode applied: %s\n", modeName(m));
}

// AudioManager's boot-load progress -> barrel boot progress bar.
//   phase 0     = SD card mounted            -> ring 0's final (5th) step
//   phase 1/2/3 = idle/fire/cooldown.wav -> PSRAM, streaming -> rings 1/2/3
void onAudioBootProgress(uint8_t phase, float fraction) {
    if (phase == 0) {
        // Only advance ring 0 past the motor step if the motor actually came
        // up - otherwise leave the bar frozen on the step that failed.
        if (motorInitOk) ledManager.setBootRingProgress(0, 1.0f);
        return;
    }
    ledManager.setBootRingProgress(phase, fraction); // phase 1 -> ring 1, etc.
}

void setup() {
    Serial.begin(115200);
    delay(200); // give native USB CDC time to enumerate before first prints
    Serial.println();
    Serial.printf("[System] Sentinel Beam firmware v%s booting...\n", FIRMWARE_VERSION);

    // Barrel LEDs first, so every later boot step can report progress on them.
    ledManager.begin();
    ledManager.beginBootSequence();
    ledManager.setBootRingProgress(0, 1.0f / 5.0f); // ring 0 step 1: LED subsystem up

    triggerManager.begin();
    modeManager.begin(); // second button (GPIO1) - part of the same input bring-up step
    ledManager.setBootRingProgress(0, 2.0f / 5.0f); // step 2: trigger + mode button

    fogManager.begin();
    ledManager.setBootRingProgress(0, 3.0f / 5.0f); // step 3: fog

    motorInitOk = motorManager.begin();
    if (motorInitOk) ledManager.setBootRingProgress(0, 4.0f / 5.0f); // step 4: motor
    // step 5 (SD mount) is filled by onAudioBootProgress() during audioManager.begin()

    audioManager.setBootProgressCallback(onAudioBootProgress);
    bool audioOk = audioManager.begin(); // SD mount + the 3 WAV loads -> rings 0(final)/1/2/3

    // The debug console must come up even on a failed boot - loop() keeps
    // pumping debugConsole.update() so a dead board can still be inspected.
    settingsController.begin(&ledManager, &audioManager, &modeManager);
    debugConsole.begin(&ledManager, &audioManager, &settingsController, &modeManager);

    if (motorInitOk && audioOk) {
        ledManager.setBootRingProgress(4, 1.0f); // ring 4: settings + console up (headroom for future subsystems)
        systemReady = true;
        currentState = STATE_IDLE;
        ledManager.bootFlash(BootResult::OK); // green x1
        ledManager.endBootSequence();
        audioManager.playIdleLoop();
        Serial.println("[System] All checks passed - idle loop started, ready to fire.");
    } else {
        systemReady = false;
        Serial.print("[System] INIT FAILED (motor=");
        Serial.print(motorInitOk ? "OK" : "FAILED");
        Serial.print(", audio=");
        Serial.print(audioOk ? "OK" : "FAILED");
        Serial.println(") - trigger input disabled, staying inert.");
        ledManager.bootFlash(BootResult::HARD_FAIL); // red x2; frozen bars stay lit as a diagnostic
    }
}

void updateTrigger() { triggerManager.update(); }
void updateMode()    { modeManager.update(); }
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

    updateMode();
    if (modeManager.mode() != appliedMode) {
        appliedMode = modeManager.mode();
        applyMode(appliedMode); // button press or `ledMode=` both land here
    }

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
