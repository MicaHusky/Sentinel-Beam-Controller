// =============================================================================
// PCBTest_03_SDBoot_BarrelLED_Audio
//
// Minimal bring-up test - NOT part of the main Sentinel Beam firmware.
// Builds on PCBTest_02: same SD-to-PSRAM check and onboard+barrel LED
// mirroring, but once fire.wav is loaded it also plays it ONCE through the
// PCM5102A DAC - so this test additionally exercises the I2S wiring.
//
// Pins mirror the main firmware's Config.h exactly, so a working result here
// carries straight over once you're back on the real sketch.
//
// Sequence (onboard LED + barrel strip always match):
//   Boot                          -> RED
//   Reading fire.wav from SD      -> YELLOW
//   Loaded into PSRAM, SD happy   -> GREEN
//   fire.wav playing              -> (stays GREEN while it plays)
//   Playback finished             -> BLUE
//   Any failure along the way     -> RED (stays), reason printed to Serial
//
// This is a one-shot test: loop() only exists to keep feeding the audio
// decoder until playback naturally ends, then does nothing further.
// =============================================================================

#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <Adafruit_NeoPixel.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

// ---- Pins (same as the main firmware's Config.h) ----
constexpr uint8_t PIN_SD_CS   = 10;
constexpr uint8_t PIN_SD_SCK  = 5;
constexpr uint8_t PIN_SD_MISO = 6;
constexpr uint8_t PIN_SD_MOSI = 7;

constexpr uint8_t PIN_I2S_DOUT = 11; // -> DIN on PCM5102A
constexpr uint8_t PIN_I2S_BCLK = 12;
constexpr uint8_t PIN_I2S_LRCK = 13;
constexpr float    AUDIO_GAIN  = 0.15f;

constexpr uint8_t PIN_ONBOARD_LED = 48; // onboard WS2812 on most ESP32-S3 dev boards
constexpr uint8_t PIN_BARREL_LED  = 18; // barrel strip data line

// Barrel strip layout matches the main firmware's Config.h - 160 physical,
// only the first 85 are ever actually used/lit.
constexpr uint16_t BARREL_TOTAL_COUNT  = 160;
constexpr uint16_t BARREL_ACTIVE_COUNT = 85;

constexpr const char* FIRE_WAV_PATH = "/fire.wav";

Adafruit_NeoPixel onboardLed(1, PIN_ONBOARD_LED, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel barrelStrip(BARREL_TOTAL_COUNT, PIN_BARREL_LED, NEO_GRB + NEO_KHZ800);

AudioOutputI2S*         audioOut  = nullptr;
AudioFileSourcePROGMEM* audioFile = nullptr;
AudioGeneratorWAV*      audioWav  = nullptr;

bool audioPlaying = false;

void setColor(uint8_t r, uint8_t g, uint8_t b) {
    onboardLed.setPixelColor(0, onboardLed.Color(r, g, b));
    onboardLed.show();

    uint32_t c = barrelStrip.Color(r, g, b);
    for (uint16_t i = 0; i < BARREL_ACTIVE_COUNT; i++) {
        barrelStrip.setPixelColor(i, c);
    }
    barrelStrip.show();
}

void setup() {
    Serial.begin(115200);
    delay(200); // give native USB CDC time to enumerate before first prints
    Serial.println();
    Serial.println("[PCBTest] SD -> PSRAM -> playback bring-up test starting...");

    onboardLed.begin();
    barrelStrip.begin();
    barrelStrip.clear(); // LEDs beyond BARREL_ACTIVE_COUNT are never touched - stay off
    barrelStrip.show();

    setColor(255, 0, 0); // BOOT
    Serial.println("[PCBTest] Boot - LEDs RED");
    delay(300); // hold red briefly so it's visibly distinct from the next step

    setColor(255, 255, 0); // READING
    Serial.println("[PCBTest] Reading fire.wav from SD into PSRAM - LEDs YELLOW");

    SPIClass sdSPI(HSPI);
    sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    if (!SD.begin(PIN_SD_CS, sdSPI)) {
        Serial.println("[PCBTest] FAILED: SD.begin() - check wiring, CS pin, and that a card is seated");
        setColor(255, 0, 0);
        return;
    }
    Serial.println("[PCBTest] SD card mounted OK");

    File f = SD.open(FIRE_WAV_PATH, FILE_READ);
    if (!f) {
        Serial.println("[PCBTest] FAILED: could not open /fire.wav - check the file is present at the card's root");
        setColor(255, 0, 0);
        return;
    }

    size_t fileSize = f.size();
    Serial.printf("[PCBTest] fire.wav found, %u bytes\n", (unsigned)fileSize);

    uint8_t* buffer = (uint8_t*)heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        Serial.println("[PCBTest] FAILED: PSRAM allocation failed - check PSRAM is enabled in board settings (OPI PSRAM) and wired correctly");
        f.close();
        setColor(255, 0, 0);
        return;
    }

    size_t readBytes = f.read(buffer, fileSize);
    f.close();

    if (readBytes != fileSize) {
        Serial.printf("[PCBTest] FAILED: short read (%u / %u bytes) - card may be failing mid-read\n",
                      (unsigned)readBytes, (unsigned)fileSize);
        heap_caps_free(buffer);
        setColor(255, 0, 0);
        return;
    }

    Serial.println("[PCBTest] Loaded into PSRAM, SD card happy - LEDs GREEN");
    setColor(0, 255, 0); // LOADED

    // ---- Start playback (I2S / DAC bring-up) ----
    audioOut = new AudioOutputI2S();
    audioOut->SetPinout(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
    audioOut->SetGain(AUDIO_GAIN);

    audioFile = new AudioFileSourcePROGMEM(buffer, fileSize);
    audioWav  = new AudioGeneratorWAV();

    if (!audioWav->begin(audioFile, audioOut)) {
        Serial.println("[PCBTest] FAILED: could not start WAV playback - check I2S wiring and that fire.wav is a valid WAV file");
        setColor(255, 0, 0);
        return;
    }

    Serial.println("[PCBTest] Playing fire.wav once - LEDs staying GREEN until it finishes");
    audioPlaying = true;
}

void loop() {
    if (!audioPlaying) {
        return; // either never started, or already finished - nothing more to do
    }

    if (audioWav->isRunning()) {
        if (!audioWav->loop()) {
            // loop() returned false - playback reached EOF (or hit an error)
            audioWav->stop();
            audioPlaying = false;
            Serial.println("[PCBTest] Playback finished - LEDs BLUE");
            setColor(0, 0, 255); // DONE
        }
    } else {
        // Defensive: isRunning() already false without us catching the EOF above
        audioPlaying = false;
        Serial.println("[PCBTest] Playback finished - LEDs BLUE");
        setColor(0, 0, 255); // DONE
    }
}
