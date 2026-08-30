# Sentinel Beam Firmware

Firmware for a full-scale, functional Halo Infinite Sentinel Beam prop, running
on an ESP32-S3. Coordinates a trigger input, PSRAM-cached audio playback, a
stepper-driven rotating barrel, three WS2812 LED zones, and a fogger.

## Getting this into Arduino IDE

Clone this repo directly into your Arduino sketchbook folder instead of
downloading zips:

```
cd ~/Documents/Arduino
git clone <this-repo-url> SentinelBeam
```

Open it in Arduino IDE like any other local sketch. To pick up a new version
later, just:

```
cd ~/Documents/Arduino/SentinelBeam
git pull
```

No re-download, no re-extract - Arduino IDE just reads whatever's on disk.

## Board settings

- Board: **ESP32S3 Dev Module**
- Flash size: **16 MB**
- PSRAM: **OPI PSRAM** (must be enabled here or nothing will boot)
- USB CDC: **Enabled**

## Required libraries (Library Manager)

- FastAccelStepper
- ESP8266Audio
- Adafruit NeoPixel
- SD / SPI (bundled with the ESP32 core)

## Pinout

| Function | Pin(s) |
|---|---|
| Trigger | GPIO4 (INPUT_PULLUP) |
| microSD (boot-time only) | CS 10, SCK 5, MISO 6, MOSI 7 |
| I2S DAC (PCM5102A) | DOUT 11, BCLK 12, LRCK 13 |
| Stepper (TMC2209) | STEP 15, DIR 16, EN 17 (active LOW) |
| Barrel LED strip | GPIO18 (160 physical / 85 active) |
| Fogger MOSFET | GPIO9 (active HIGH) |
| Vent LED strip (left) | GPIO41 (80 LEDs) |
| Vent LED strip (right) | GPIO40 (80 LEDs) |

Reserved, do not use: GPIO19/20 (USB D-/D+), ~GPIO26-32 (OPI PSRAM),
GPIO0/3/45/46 (strapping pins).

## SD card contents

Place at the root of the card:
- `idle.wav`
- `fire.wav`
- `cooldown.wav` (a single file containing 5 distinct "ka-chunk" hits)

## Boot indicator

The barrel doubles as a boot progress bar. Each of its five 17-LED rings fills
in cyan as one phase of startup completes:

| Ring | Phase |
|---|---|
| 1 | Subsystem inits (LEDs, trigger, fog, motor, SD card mount) |
| 2 | `idle.wav` copied into PSRAM |
| 3 | `fire.wav` copied into PSRAM |
| 4 | `cooldown.wav` copied into PSRAM |
| 5 | Everything after (settings, debug console; reserved for future subsystems) |

Rings 2-4 sweep as the audio files stream off the card. When it finishes, the
whole barrel flashes the result: **green once** = all checks passed (idle loop
starts), **red twice** = a required subsystem failed and the firmware is
staying inert - the bars freeze where they stalled, so the unfilled ring shows
you which subsystem to look at. (A yellow-twice "degraded mode" code is
reserved for later.)

## Debug console

Open the Serial Monitor at 115200 baud. Type `help` for the full list of
live-tunable settings (colors, timing, brightness, rainbow mode). Changes are
runtime-only - they reset to the defaults in `Config.h` on the next power
cycle.

## Repo layout

- `SentinelBeam.ino` + the various `*Manager` classes - the actual firmware.
- `bring-up-tests/` - minimal standalone sketches used to test each subsystem
  on a freshly populated PCB, one at a time, independent of the main firmware.

## Versioning

See `CHANGELOG.md`. The running firmware version prints over Serial at boot.
