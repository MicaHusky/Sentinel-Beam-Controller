#pragma once
#include <Arduino.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include "Config.h"

// =============================================================================
// AudioManager
//
// At begin(): mounts the SD card ONE TIME, copies idle.wav, fire.wav, and
// cooldown.wav entirely into PSRAM, then fully shuts the SD card down. From
// that point on the SD card is never touched again - all playback reads out
// of PSRAM.
//
// Three tracks, one at a time:
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
    COOLDOWN
};

struct WavAsset {
    uint8_t* buffer = nullptr;
    size_t   size   = 0;
    bool     loaded = false;
};

class AudioManager {
public:
    // Optional boot-time load-progress hook, used by the main sketch to drive
    // the barrel's boot progress bar. phase: 0 = SD card mounted, 1 = idle.wav,
    // 2 = fire.wav, 3 = cooldown.wav; fraction is 0..1 within that phase
    // (always 1.0 for phase 0). AudioManager itself knows nothing about the
    // LEDs - it only emits progress. Set this before begin().
    using BootProgressFn = void (*)(uint8_t phase, float fraction);
    void setBootProgressCallback(BootProgressFn cb) { _bootProgressCb = cb; }

    bool begin(); // load idle/fire/cooldown into PSRAM; false if ANY of them fail

    void playIdleLoop();
    void playFire();
    void playCooldown();
    void stopAll();

    void update(); // call once per loop() - drives the active decoder

    // Each true for exactly one update() call, the moment that track
    // finishes on its own.
    bool fireReachedEOF()     const { return _fireEOF; }
    bool cooldownReachedEOF() const { return _cooldownEOF; }

    AudioTrack activeTrack() const { return _activeTrack; }

    // Runtime-tunable playback gain (Config.h's AUDIO_GAIN is only the boot
    // default) - this is what DebugConsole's audioGain= command adjusts.
    void setGain(float gain);
    float getGain() const { return _gain; }

private:
    bool loadAssetToPSRAM(const char* path, WavAsset& asset, uint8_t progressPhase);
    void startTrack(WavAsset& asset, AudioTrack track);
    void teardownPlayback();

    WavAsset _idleAsset;
    WavAsset _fireAsset;
    WavAsset _cooldownAsset;

    AudioOutputI2S*          _out  = nullptr;
    AudioFileSourcePROGMEM*  _file = nullptr;
    AudioGeneratorWAV*       _wav  = nullptr;

    AudioTrack _activeTrack = AudioTrack::NONE;
    bool _fireEOF     = false;
    bool _cooldownEOF = false;
    float _gain = AUDIO_GAIN;
    BootProgressFn _bootProgressCb = nullptr;
};
