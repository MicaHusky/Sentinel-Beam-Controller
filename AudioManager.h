#pragma once
#include <Arduino.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <SPI.h>
#include "Config.h"

// =============================================================================
// AudioManager
//
// At begin(): mounts the SD card, copies the stock idle.wav, fire.wav, and
// cooldown.wav (card root) entirely into PSRAM, then fully shuts the SD card
// down. All playback reads out of PSRAM - the card is NEVER read during
// playback.
//
// The only other time the card is touched is an explicit mode switch (GPIO1
// mode button / console `mode=`), which the main sketch only performs while
// the beam is IDLE: it calls mountSD(), reads the mode's mode.txt with
// readTextFile(), checks the WAVs exist, then loadSoundSet() frees the three
// current PSRAM buffers before copying in the new folder's set, and finally
// unmountSD(). AudioManager owns the SD bus because it already owned it for
// the boot load - it only hands back file contents, it doesn't interpret
// mode.txt (that's SettingsController's job, via the main sketch).
//
// Display mode swaps in a different pair: /DisplayMode/idle.wav (as IDLE) and
// activity.wav (ACTIVITY, one-shot) - see loadDisplaySet(). fire/cooldown are
// left unloaded there, since the trigger is ignored in display mode.
//
// Tracks, one at a time:
//   - IDLE     : loops forever (restarted from PSRAM the instant it hits EOF)
//   - FIRE     : one-shot, reachedEOF reported via fireReachedEOF()
//   - COOLDOWN : one-shot, reachedEOF reported via cooldownReachedEOF()
//
// Switching tracks always tears down whatever is currently playing first, so
// e.g. playFire() called mid-cooldown restarts fire.wav from sample zero
// immediately.
// =============================================================================

enum class AudioTrack {
    NONE,
    IDLE,
    FIRE,
    COOLDOWN,
    ACTIVITY
};

struct WavAsset {
    uint8_t* buffer = nullptr;
    size_t   size   = 0;
    bool     loaded = false;
};

class AudioManager {
public:
    // Optional load-progress hook. At boot the main sketch uses it to drive
    // the barrel's boot progress bar; during a mode switch it swaps in one
    // that keeps the LEDs (status indicator) animating while the load blocks.
    // phase: 0 = SD card mounted, 1 = idle.wav, 2 = fire.wav,
    // 3 = cooldown.wav; fraction is 0..1 within that phase (always 1.0 for
    // phase 0). AudioManager itself knows nothing about the LEDs - it only
    // emits progress.
    using BootProgressFn = void (*)(uint8_t phase, float fraction);
    void setBootProgressCallback(BootProgressFn cb) { _bootProgressCb = cb; }

    bool begin(); // load the stock idle/fire/cooldown into PSRAM; false if ANY of them fail

    // ---- SD access for mode switches (main sketch only calls these while IDLE) ----
    bool mountSD();
    void unmountSD();
    bool fileExists(const String& path);
    bool readTextFile(const String& path, String& out, size_t maxBytes); // false if missing/too big
    // Stops playback, frees the current three PSRAM buffers, then loads
    // <dir>/idle.wav, fire.wav, cooldown.wav (dir "" = card root). SD must be
    // mounted. False if ANY failed (those tracks are left unloaded/silent).
    bool loadSoundSet(const String& dir);
    // Same, but each track from its own folder - used when an alt mode is
    // missing some WAVs and borrows the stock ones from the root ("").
    bool loadSoundSet(const String& idleDir, const String& fireDir, const String& cooldownDir);
    // Display mode: frees everything, loads <dir>/idle.wav + <dir>/activity.wav.
    bool loadDisplaySet(const String& dir);

    void playIdleLoop();
    void playFire();
    void playCooldown();
    void playActivity(); // display mode
    void stopAll();

    void update(); // call once per loop() - drives the active decoder

    // Each true for exactly one update() call, the moment that track
    // finishes on its own.
    bool fireReachedEOF()     const { return _fireEOF; }
    bool cooldownReachedEOF() const { return _cooldownEOF; }
    bool activityReachedEOF() const { return _activityEOF; }

    AudioTrack activeTrack() const { return _activeTrack; }

    // Runtime-tunable playback gain (Config.h's AUDIO_GAIN is only the boot
    // default) - this is what DebugConsole's audioGain= command adjusts.
    void setGain(float gain);
    float getGain() const { return _gain; }

private:
    bool loadAssetToPSRAM(const String& path, WavAsset& asset, uint8_t progressPhase);
    static void freeAsset(WavAsset& asset);
    void startTrack(WavAsset& asset, AudioTrack track);
    void teardownPlayback();

    WavAsset _idleAsset;
    WavAsset _fireAsset;
    WavAsset _cooldownAsset;
    WavAsset _activityAsset; // display mode only

    AudioOutputI2S*          _out  = nullptr;
    AudioFileSourcePROGMEM*  _file = nullptr;
    AudioGeneratorWAV*       _wav  = nullptr;

    AudioTrack _activeTrack = AudioTrack::NONE;
    bool _fireEOF     = false;
    bool _cooldownEOF = false;
    bool _activityEOF = false;
    float _gain = AUDIO_GAIN;
    BootProgressFn _bootProgressCb = nullptr;
    SPIClass _sdSPI{HSPI};
    bool _sdMounted = false;
};
