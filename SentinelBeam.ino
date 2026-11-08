// Sentinel Beam firmware  v1.5.0  -  display mode: 5 clicks, idle loop + random activity.wav with slow barrel twitches
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
// Modes:   GPIO1 single/double/triple press -> /AltMode1/2/3 on the SD card
//          (its own idle/fire/cooldown.wav + mode.txt settings); long press
//          -> stock. Requests made while firing/cooling down are queued and
//          applied once IDLE. See loadProfile().
// Display: 5 presses -> /DisplayMode/: idle.wav loops, trigger + fog ignored,
//          activity.wav plays at random intervals with slow random barrel
//          twitches (DisplayModeManager). Hold the mode button ~5 s to exit.
// =============================================================================

#include "Config.h"
#include "TriggerManager.h"
#include "ModeManager.h"
#include "MotorManager.h"
#include "AudioManager.h"
#include "LEDManager.h"
#include "FogManager.h"
#include "DisplayModeManager.h"
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
DisplayModeManager displayModeManager;
SettingsController settingsController;
DebugConsole   debugConsole;

SystemState currentState = STATE_INIT;
bool systemReady = false;
bool motorInitOk = false; // latched from motorManager.begin(); gates boot ring 0's SD-mount step

// Every runtime-tunable setting back to its Config.h stock value - what a
// long press means by "as if the Sentinel Beam has just booted". Each alt
// mode also starts from here before applying its mode.txt on top.
void resetAllSettingsToStock() {
    ledManager.resetToDefaults();
    audioManager.setGain(AUDIO_GAIN);
    modeManager.resetToDefaults();
    displayModeManager.resetToDefaults();
}

// Parse an alt mode's mode.txt and push each key=value line through
// SettingsController - the same validation the debug console uses. Blank
// lines and lines starting with '#' or '//' are ignored. Returns false (with
// a reason) on the first bad line; the caller then rejects the whole mode.
bool applyModeFile(const String& text, const String& path, String& reason) {
    int lineNo = 0;
    int start = 0;
    // Skip a UTF-8 byte-order mark (some Windows editors add one), or the
    // first line would read as garbage and reject the whole mode.
    if (text.length() >= 3 && (uint8_t)text[0] == 0xEF && (uint8_t)text[1] == 0xBB && (uint8_t)text[2] == 0xBF) {
        start = 3;
    }
    const int len = text.length();
    while (start <= len) {
        int nl = text.indexOf('\n', start);
        if (nl < 0) nl = len;
        String line = text.substring(start, nl);
        start = nl + 1;
        lineNo++;

        line.trim(); // also strips a trailing '\r' from Windows line endings
        if (line.length() == 0 || line.startsWith("#") || line.startsWith("//")) continue;

        int eq = line.indexOf('=');
        if (eq <= 0) {
            reason = path + " line " + lineNo + ": expected key=value, got '" + line + "'";
            return false;
        }
        SettingResult r = settingsController.apply(line.substring(0, eq), line.substring(eq + 1), true);
        if (!r.ok) {
            reason = path + " line " + lineNo + ": " + r.message;
            return false;
        }
        Serial.printf("[System]   %s line %d: %s\n", path.c_str(), lineNo, r.message.c_str());
    }
    return true;
}

// Keeps the LEDs (in particular the status indicator's blink) animating while
// a mode switch blocks on the SD card. Throttled - a show() of every strip
// per 16 KB chunk would slow the load for no visible gain.
void onModeLoadProgress(uint8_t /*phase*/, float /*fraction*/) {
    static unsigned long lastMs = 0;
    if (millis() - lastMs >= 20) {
        lastMs = millis();
        ledManager.update();
    }
}

// Stock: root WAVs + Config.h defaults. Used by a long press and as the
// fallback for a rejected alt mode. SD must already be mounted.
bool loadStockFromMountedSD() {
    resetAllSettingsToStock();
    return audioManager.loadSoundSet("");
}

// Load an alt mode. SD must already be mounted. Any of the folder's four
// files may be missing:
//   - a missing WAV is replaced by the stock one from the card root
//     (usedRootSounds -> the status indicator shows turquoise, not green)
//   - a missing mode.txt means stock settings
//   - ALL FOUR missing (empty folder, no folder, or misnamed files) is
//     treated as a mistake: rejected, red flash, revert to stock.
// A mode.txt that IS present but has a bad line still rejects the mode.
// Everything is validated BEFORE the current sounds are freed from PSRAM.
bool loadAltFromMountedSD(ModeProfile p, String& reason, bool& usedRootSounds) {
    const String dir = String(ALT_MODE_DIR_PREFIX) + (int)p;
    const String cfgPath = dir + "/" + MODE_CONFIG_FILENAME;
    usedRootSounds = false;

    const char* wavs[3] = {IDLE_WAV_PATH, FIRE_WAV_PATH, COOLDOWN_WAV_PATH};
    String wavDir[3];
    uint8_t present = 0;
    for (uint8_t i = 0; i < 3; i++) {
        if (audioManager.fileExists(dir + wavs[i])) {
            wavDir[i] = dir;
            present++;
        } else {
            wavDir[i] = ""; // card root
            usedRootSounds = true;
            Serial.printf("[System]   %s%s not found - using stock %s from the card root\n",
                          dir.c_str(), wavs[i], wavs[i]);
        }
    }
    const bool hasCfg = audioManager.fileExists(cfgPath);

    if (present == 0 && !hasCfg) {
        reason = dir + " is missing, empty, or has no correctly named files";
        return false;
    }

    resetAllSettingsToStock(); // alt modes always start from stock, never from the previous mode

    if (hasCfg) {
        String text;
        if (!audioManager.readTextFile(cfgPath, text, MODE_CONFIG_MAX_BYTES)) {
            reason = cfgPath + " unreadable or larger than " + String((unsigned)MODE_CONFIG_MAX_BYTES) + " bytes";
            return false;
        }
        if (!applyModeFile(text, cfgPath, reason)) return false;
    } else {
        Serial.printf("[System]   %s not found - using stock settings\n", cfgPath.c_str());
    }

    if (!audioManager.loadSoundSet(wavDir[0], wavDir[1], wavDir[2])) {
        reason = "couldn't load " + dir + "'s sound set into PSRAM (see above)";
        return false;
    }
    return true;
}

// Load display mode. SD must already be mounted. Unlike the alt modes there's
// no per-file fallback: ALL of DisplayMode.txt, idle.wav, and activity.wav
// must be present or the switch is rejected (red flash, revert to stock).
// DisplayMode.txt takes every mode.txt key plus the display-only ones.
bool loadDisplayFromMountedSD(String& reason) {
    const String dir = DISPLAY_MODE_DIR;
    const String cfgPath = dir + "/" + DISPLAY_MODE_CONFIG_FILE;

    String missing;
    if (!audioManager.fileExists(cfgPath))                missing += " " + cfgPath;
    if (!audioManager.fileExists(dir + IDLE_WAV_PATH))     missing += " " + dir + IDLE_WAV_PATH;
    if (!audioManager.fileExists(dir + ACTIVITY_WAV_PATH)) missing += " " + dir + ACTIVITY_WAV_PATH;
    if (missing.length()) {
        reason = "display mode needs all of its files - missing:" + missing;
        return false;
    }

    String text;
    if (!audioManager.readTextFile(cfgPath, text, MODE_CONFIG_MAX_BYTES)) {
        reason = cfgPath + " unreadable or larger than " + String((unsigned)MODE_CONFIG_MAX_BYTES) + " bytes";
        return false;
    }

    resetAllSettingsToStock();
    if (!applyModeFile(text, cfgPath, reason)) return false;

    if (!audioManager.loadDisplaySet(dir)) {
        reason = "couldn't load the display-mode WAVs into PSRAM (see above)";
        return false;
    }
    return true;
}

// Perform a mode switch. Only called from loop() while STATE_IDLE. Blocks
// for as long as the SD reads take; the status indicator keeps blinking via
// onModeLoadProgress(). If an alt mode is rejected (see
// loadAltFromMountedSD()) the status LEDs flash red and the prop reverts to
// stock. If it borrowed any root WAVs, the indicator turns turquoise.
void loadProfile(ModeProfile p) {
    Serial.printf("[System] mode switch -> %s\n", profileName(p));
    ledManager.showModeIndicator((uint8_t)p); // already showing if requested via the button; harmless repeat
    if (displayModeManager.isActive()) {
        displayModeManager.exit();
        motorManager.stopTwitch(); // controlled decel; limp once stopped (harmless if already still)
    }
    audioManager.stopAll();
    audioManager.setBootProgressCallback(onModeLoadProgress);

    ModeProfile applied = p;
    bool ok = false;
    bool usedRootSounds = false;
    String reason;

    if (!audioManager.mountSD()) {
        reason = "SD card could not be mounted";
    } else if (p == PROFILE_STOCK) {
        ok = loadStockFromMountedSD();
        if (!ok) reason = "stock WAVs at the card root failed to load";
    } else if (p == PROFILE_DISPLAY) {
        ok = loadDisplayFromMountedSD(reason);
    } else {
        ok = loadAltFromMountedSD(p, reason, usedRootSounds);
    }

    if (!ok) {
        Serial.printf("[System] MODE SWITCH REJECTED (%s): %s - reverting to stock\n",
                      profileName(p), reason.c_str());
        ledManager.showFaultIndicator();
        applied = PROFILE_STOCK;
        if (p != PROFILE_STOCK && audioManager.mountSD()) {
            if (!loadStockFromMountedSD()) {
                Serial.println("[System] ERROR: stock WAVs also failed to load - running without sound");
            }
        } else {
            // SD unavailable (or stock itself failed): settings still go back
            // to stock; whatever audio is in PSRAM (possibly none) stays.
            resetAllSettingsToStock();
            Serial.println("[System] ERROR: could not (re)load stock sounds from SD - audio may be silent");
        }
    } else {
        ledManager.releaseModeIndicator(usedRootSounds);
        Serial.printf("[System] mode switch complete: %s%s\n", profileName(p),
                      usedRootSounds ? " (using some stock sounds from the card root)" : "");
    }

    audioManager.unmountSD();
    audioManager.setBootProgressCallback(nullptr);
    modeManager.markApplied(applied);
    if (applied == PROFILE_DISPLAY) displayModeManager.enter();
    audioManager.playIdleLoop();
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
    audioManager.setBootProgressCallback(nullptr); // boot bar is done; mode switches install their own

    // The debug console must come up even on a failed boot - loop() keeps
    // pumping debugConsole.update() so a dead board can still be inspected.
    settingsController.begin(&ledManager, &audioManager, &modeManager, &displayModeManager);
    debugConsole.begin(&ledManager, &audioManager, &settingsController, &modeManager, &displayModeManager);

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

// Display mode: turn DisplayModeManager's events into audio/motor calls.
// The fogger is never touched here, and the trigger isn't read at all.
void updateDisplayMode() {
    if (audioManager.activityReachedEOF()) {
        if (displayModeManager.notifyActivityEnded()) motorManager.stopTwitch();
        audioManager.playIdleLoop(); // restart the idle loop from the top
    }

    const DisplayEvent ev = displayModeManager.update();
    switch (ev) {
        case DisplayEvent::START_ACTIVITY:
            audioManager.playActivity();
            if (audioManager.activeTrack() != AudioTrack::ACTIVITY) {
                // Shouldn't happen (activity.wav is required to enter display
                // mode), but never leave the scheduler waiting on an EOF that
                // won't come.
                displayModeManager.notifyActivityEnded();
                audioManager.playIdleLoop();
            }
            break;
        case DisplayEvent::TWITCH_FORWARD:
        case DisplayEvent::TWITCH_REVERSE:
            motorManager.startTwitch(ev == DisplayEvent::TWITCH_FORWARD,
                                     displayModeManager.twitchSpeedHz(),
                                     displayModeManager.twitchAccelHz());
            break;
        case DisplayEvent::TWITCH_STOP:
            motorManager.stopTwitch();
            break;
        case DisplayEvent::NONE:
        default:
            break;
    }
}

// Future: updateRumble()

void loop() {
    debugConsole.update(); // always available, even if boot failed

    if (!systemReady) {
        return; // see the INIT FAILED message printed once in setup()
    }

    updateTrigger();

    updateMode();
    if (modeManager.takeRequestJustMade()) {
        // Immediate feedback for a button press / `mode=`, even if the switch
        // itself has to wait for firing/cooldown to finish.
        ledManager.showModeIndicator((uint8_t)modeManager.pending());
        if (currentState != STATE_IDLE) {
            Serial.printf("[System] %s queued - will load once back at IDLE\n", profileName(modeManager.pending()));
        }
    }
    if (modeManager.hasPending() && currentState == STATE_IDLE) {
        loadProfile(modeManager.takePending()); // blocking SD load; button press or `mode=` both land here
    }

    updateAudio();
    updateMotor();
    updateLEDs();

    if (displayModeManager.isActive()) {
        updateDisplayMode();
        return; // trigger (and with it the fogger) is ignored in display mode
    }

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
