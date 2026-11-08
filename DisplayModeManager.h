#pragma once
#include <Arduino.h>

// =============================================================================
// DisplayModeManager
//
// The "display mode" scheduler (5 clicks of the mode button; /DisplayMode/ on
// the SD card). While active, the prop sits on a table looping idle.wav with
// the trigger and fogger ignored. Every activityMin..activityMaxSeconds (random)
// it decides it's time for "activity": the main sketch plays activity.wav
// (interrupting the idle loop) and this class sequences a burst of
// twitchCountMin..twitchCountMax slow barrel twitches - each in a random
// direction, for a random twitchMin..twitchMaxSeconds, separated by random
// twitchPauseMin..twitchPauseMaxSeconds rests - at twitchRPM with
// twitchAccelPercent of the normal firing acceleration.
//
// The burst is clipped to activity.wav: when the clip ends the main sketch
// calls notifyActivityEnded(), any twitch in progress is told to stop (a
// controlled decel), idle.wav restarts, and the next interval is drawn from
// that moment. Between twitches the motor is stopped and, as everywhere else,
// the stepper EN auto-disables the moment it reaches standstill - the barrel
// is limp at rest.
//
// Like every other manager this one never calls another: update() returns one
// event per call and the main sketch translates it into AudioManager /
// MotorManager calls. All bounds are runtime-tunable (console or the
// DisplayMode.txt file, via SettingsController); each range is used as
// min(a,b)..max(a,b), so setting the two ends in either order is fine.
// =============================================================================

enum class DisplayEvent {
    NONE,
    START_ACTIVITY,    // play activity.wav
    TWITCH_FORWARD,    // start a twitch, forward
    TWITCH_REVERSE,    // start a twitch, reverse
    TWITCH_STOP        // decelerate to a stop (EN auto-disables at standstill)
};

class DisplayModeManager {
public:
    DisplayModeManager() { resetToDefaults(); }

    void enter();  // display mode just loaded - schedule the first activity
    void exit();   // leaving display mode (caller stops the motor)
    bool isActive() const { return _active; }

    DisplayEvent update(); // call once per loop() while active
    // activity.wav finished. Returns true if a twitch was still running (the
    // caller should stop the motor).
    bool notifyActivityEnded();

    // ---- runtime-tunable settings (Config.h values are the defaults) ----
    void resetToDefaults();
    float activityMinSeconds = 0, activityMaxSeconds = 0;
    uint8_t twitchCountMin = 0, twitchCountMax = 0;
    float twitchMinSeconds = 0, twitchMaxSeconds = 0;
    float twitchPauseMinSeconds = 0, twitchPauseMaxSeconds = 0;
    float twitchRPM = 0;
    float twitchAccelPercent = 0; // % of MOTOR_ACCEL_HZ_S2

    // Motor parameters derived from the above, for MotorManager::startTwitch().
    uint32_t twitchSpeedHz() const;
    uint32_t twitchAccelHz() const;

private:
    enum class Phase { WAITING, TWITCHING, RESTING, ACTIVITY_TAIL };

    static uint32_t randomMs(float aSec, float bSec);
    void scheduleNextActivity();

    bool _active = false;
    Phase _phase = Phase::WAITING;
    unsigned long _phaseUntilMs = 0;
    uint8_t _twitchesLeft = 0;
};
