#include "AudioManager.h"
#include "Config.h"
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>

bool AudioManager::begin() {
    _out = new AudioOutputI2S();
    _out->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
    _out->SetGain(_gain);

    if (!mountSD()) return false;
    bool ok = loadSoundSet("");
    unmountSD(); // card goes idle again until/unless a mode switch is requested
    return ok;
}

bool AudioManager::mountSD() {
    if (_sdMounted) return true;
    _sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS, _sdSPI)) {
        Serial.println("[AudioManager] SD.begin() FAILED - check card / wiring / CS pin");
        _sdSPI.end();
        return false;
    }
    _sdMounted = true;
    Serial.println("[AudioManager] SD card mounted");
    if (_bootProgressCb) _bootProgressCb(0, 1.0f); // SD mounted (boot: ring 0's final step)
    return true;
}

void AudioManager::unmountSD() {
    if (!_sdMounted) return;
    SD.end();
    _sdSPI.end();
    _sdMounted = false;
    Serial.println("[AudioManager] SD card unmounted (idle)");
}

bool AudioManager::fileExists(const String& path) {
    if (!_sdMounted) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    bool isFile = !f.isDirectory();
    f.close();
    return isFile;
}

bool AudioManager::readTextFile(const String& path, String& out, size_t maxBytes) {
    out = "";
    if (!_sdMounted) return false;
    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        Serial.printf("[AudioManager] could not open %s\n", path.c_str());
        if (f) f.close();
        return false;
    }
    if ((size_t)f.size() > maxBytes) {
        Serial.printf("[AudioManager] %s is too large (%u bytes, max %u)\n",
                      path.c_str(), (unsigned)f.size(), (unsigned)maxBytes);
        f.close();
        return false;
    }
    out.reserve(f.size() + 1);
    while (f.available()) out += (char)f.read();
    f.close();
    Serial.printf("[AudioManager] read %s (%u bytes)\n", path.c_str(), (unsigned)out.length());
    return true;
}

void AudioManager::freeAsset(WavAsset& asset) {
    if (asset.buffer) heap_caps_free(asset.buffer);
    asset.buffer = nullptr;
    asset.size = 0;
    asset.loaded = false;
}

bool AudioManager::loadSoundSet(const String& dir) {
    return loadSoundSet(dir, dir, dir);
}

bool AudioManager::loadSoundSet(const String& idleDir, const String& fireDir, const String& cooldownDir) {
    stopAll(); // nothing may be reading a buffer we're about to free
    freeAsset(_idleAsset);
    freeAsset(_fireAsset);
    freeAsset(_cooldownAsset);

    bool idleOk     = loadAssetToPSRAM(idleDir     + IDLE_WAV_PATH,     _idleAsset,     1);
    bool fireOk     = loadAssetToPSRAM(fireDir     + FIRE_WAV_PATH,     _fireAsset,     2);
    bool cooldownOk = loadAssetToPSRAM(cooldownDir + COOLDOWN_WAV_PATH, _cooldownAsset, 3);

    Serial.printf("[AudioManager] sound set: idle.wav: %s | fire.wav: %s | cooldown.wav: %s\n",
                  idleOk ? "OK" : "FAILED",
                  fireOk ? "OK" : "FAILED",
                  cooldownOk ? "OK" : "FAILED");

    return idleOk && fireOk && cooldownOk;
}

bool AudioManager::loadAssetToPSRAM(const String& pathStr, WavAsset& asset, uint8_t progressPhase) {
    const char* path = pathStr.c_str();
    File f = SD.open(pathStr, FILE_READ);
    if (!f) {
        Serial.printf("[AudioManager] could not open %s\n", path);
        return false;
    }

    asset.size = f.size();
    asset.buffer = static_cast<uint8_t*>(heap_caps_malloc(asset.size, MALLOC_CAP_SPIRAM));

    if (!asset.buffer) {
        Serial.printf("[AudioManager] PSRAM alloc FAILED for %s (%u bytes requested)\n",
                      path, (unsigned)asset.size);
        asset.size = 0;
        f.close();
        return false;
    }

    // Copy in chunks rather than one blocking f.read(), so the progress hook
    // (boot bar / status indicator) can keep animating as the bytes land.
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
        freeAsset(asset);
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
