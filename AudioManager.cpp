#include "AudioManager.h"
#include "Config.h"
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>

bool AudioManager::begin() {
    _out = new AudioOutputI2S();
    _out->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
    _out->SetGain(_gain);

    SPIClass sdSPI(HSPI);
    sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    if (!SD.begin(PIN_SD_CS, sdSPI)) {
        Serial.println("[AudioManager] SD.begin() FAILED - check wiring/CS pin");
        return false;
    }
    if (_bootProgressCb) _bootProgressCb(0, 1.0f); // SD mounted - boot ring 0's final step

    bool idleOk     = loadAssetToPSRAM(IDLE_WAV_PATH, _idleAsset, 1);
    bool fireOk     = loadAssetToPSRAM(FIRE_WAV_PATH, _fireAsset, 2);
    bool cooldownOk = loadAssetToPSRAM(COOLDOWN_WAV_PATH, _cooldownAsset, 3);

    SD.end(); // SD is never touched again after this point, pass or fail

    Serial.printf("[AudioManager] idle.wav: %s | fire.wav: %s | cooldown.wav: %s\n",
                  idleOk ? "OK" : "FAILED",
                  fireOk ? "OK" : "FAILED",
                  cooldownOk ? "OK" : "FAILED");

    return idleOk && fireOk && cooldownOk;
}

bool AudioManager::loadAssetToPSRAM(const char* path, WavAsset& asset, uint8_t progressPhase) {
    File f = SD.open(path, FILE_READ);
    if (!f) {
        Serial.printf("[AudioManager] could not open %s\n", path);
        return false;
    }

    asset.size = f.size();
    asset.buffer = static_cast<uint8_t*>(heap_caps_malloc(asset.size, MALLOC_CAP_SPIRAM));

    if (!asset.buffer) {
        Serial.printf("[AudioManager] PSRAM alloc FAILED for %s (%u bytes requested)\n",
                      path, (unsigned)asset.size);
        f.close();
        return false;
    }

    // Copy in chunks rather than one blocking f.read(), so the boot progress
    // ring for this phase can sweep as the bytes land (see AUDIO_LOAD_CHUNK_BYTES).
    size_t readBytes = 0;
    while (readBytes < asset.size) {
        size_t want = asset.size - readBytes;
        if (want > AUDIO_LOAD_CHUNK_BYTES) want = AUDIO_LOAD_CHUNK_BYTES;
        size_t got = f.read(asset.buffer + readBytes, want);
        if (got == 0) break; // short/failed read - caught by the size check below
        readBytes += got;
        if (_bootProgressCb) {
            _bootProgressCb(progressPhase, (float)readBytes / (float)asset.size);
        }
    }
    f.close();

    if (readBytes != asset.size) {
        Serial.printf("[AudioManager] short read on %s (%u/%u bytes)\n",
                      path, (unsigned)readBytes, (unsigned)asset.size);
        heap_caps_free(asset.buffer);
        asset.buffer = nullptr;
        asset.size = 0;
        return false;
    }

    asset.loaded = true;
    Serial.printf("[AudioManager] loaded %s into PSRAM (%u bytes)\n", path, (unsigned)asset.size);
    return true;
}

void AudioManager::teardownPlayback() {
    if (_wav) {
        _wav->stop();
        delete _wav;
        _wav = nullptr;
    }
    if (_file) {
        delete _file;
        _file = nullptr;
    }
}

void AudioManager::startTrack(WavAsset& asset, AudioTrack track) {
    teardownPlayback();

    if (!asset.loaded) {
        Serial.println("[AudioManager] tried to play a track that never loaded - skipping");
        _activeTrack = AudioTrack::NONE;
        return;
    }

    _file = new AudioFileSourcePROGMEM(asset.buffer, asset.size);
    _wav  = new AudioGeneratorWAV();
    _wav->begin(_file, _out);
    _activeTrack = track;
}

void AudioManager::playIdleLoop() {
    Serial.println("[AudioManager] -> idle.wav (looping)");
    startTrack(_idleAsset, AudioTrack::IDLE);
}

void AudioManager::playFire() {
    Serial.println("[AudioManager] -> fire.wav");
    startTrack(_fireAsset, AudioTrack::FIRE);
}

void AudioManager::playCooldown() {
    Serial.println("[AudioManager] -> cooldown.wav");
    startTrack(_cooldownAsset, AudioTrack::COOLDOWN);
}

void AudioManager::stopAll() {
    teardownPlayback();
    _activeTrack = AudioTrack::NONE;
}

void AudioManager::setGain(float gain) {
    _gain = gain;
    if (_out) {
        _out->SetGain(_gain);
    }
}

void AudioManager::update() {
    _fireEOF = false;
    _cooldownEOF = false;

    if (!_wav || !_wav->isRunning()) return;
    if (_wav->loop()) return; // still playing, nothing to do this cycle

    // wav->loop() returned false: the active track just reached EOF on its own.
    AudioTrack finishedTrack = _activeTrack;
    teardownPlayback();
    _activeTrack = AudioTrack::NONE;

    switch (finishedTrack) {
        case AudioTrack::IDLE:
            // This IS the loop - immediately restart from PSRAM.
            startTrack(_idleAsset, AudioTrack::IDLE);
            break;
        case AudioTrack::FIRE:
            Serial.println("[AudioManager] fire.wav reached EOF");
            _fireEOF = true;
            break;
        case AudioTrack::COOLDOWN:
            Serial.println("[AudioManager] cooldown.wav finished");
            _cooldownEOF = true;
            break;
        default:
            break;
    }
}
