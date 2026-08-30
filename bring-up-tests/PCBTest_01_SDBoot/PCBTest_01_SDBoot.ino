// =============================================================================
// PCBTest_01_SDBoot
//
// Minimal bring-up test - NOT part of the main Sentinel Beam firmware.
// Purpose: verify the SD card wiring and PSRAM are both working on a freshly
// populated board before moving on to the next subsystem.
//
// Pins mirror the main firmware's Config.h exactly, so a working result here
// carries straight over once you're back on the real sketch.
//
// Sequence:
//   Boot                          -> RED
//   Reading fire.wav from SD      -> YELLOW
//   Fully read into PSRAM, happy  -> GREEN
//   Any failure along the way     -> RED (stays), reason printed to Serial
//
// This is a one-shot test: loop() does nothing, the LED just holds whatever
// color setup() ended on.
// =============================================================================

#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <Adafruit_NeoPixel.h>

// ---- Pins (same as the main firmware's Config.h) ----
constexpr uint8_t PIN_SD_CS   = 10;
constexpr uint8_t PIN_SD_SCK  = 5;
constexpr uint8_t PIN_SD_MISO = 6;
constexpr uint8_t PIN_SD_MOSI = 7;

constexpr uint8_t PIN_ONBOARD_LED = 48; // onboard WS2812 on most ESP32-S3 dev boards

constexpr const char* FIRE_WAV_PATH = "/fire.wav";

Adafruit_NeoPixel onboardLed(1, PIN_ONBOARD_LED, NEO_GRB + NEO_KHZ800);

void setColor(uint8_t r, uint8_t g, uint8_t b) {
    onboardLed.setPixelColor(0, onboardLed.Color(r, g, b));
    onboardLed.show();
}

void setup() {
    Serial.begin(115200);
    delay(200); // give native USB CDC time to enumerate before first prints
    Serial.println();
    Serial.println("[PCBTest] SD-to-PSRAM bring-up test starting...");

    onboardLed.begin();
    setColor(255, 0, 0); // BOOT
    Serial.println("[PCBTest] Boot - LED RED");
    delay(300); // hold red briefly so it's visibly distinct from the next step

    setColor(255, 255, 0); // READING
    Serial.println("[PCBTest] Reading fire.wav from SD into PSRAM - LED YELLOW");

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

    Serial.println("[PCBTest] SUCCESS - fire.wav fully read into PSRAM, SD card happy - LED GREEN");
    setColor(0, 255, 0); // SUCCESS

    // Buffer is intentionally left allocated - this is a one-shot hardware
    // test, not the real firmware, so there's nothing further to do with it.
}

void loop() {
    // Nothing to do - the LED already reflects the final result from setup().
}
