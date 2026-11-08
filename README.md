# Sentinel Beam Firmware

Firmware for a full-scale, functional Halo Infinite Sentinel Beam prop, running
on an ESP32-S3. Coordinates a trigger input, PSRAM-cached audio playback, a
stepper-driven rotating barrel, three WS2812 LED zones (plus a 2-LED status
indicator), and a fogger.

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
| Mode button | GPIO1 (INPUT_PULLUP) |
| microSD (boot + mode switches only) | CS 10, SCK 5, MISO 6, MOSI 7 |
| I2S DAC (PCM5102A) | DOUT 11, BCLK 12, LRCK 13 |
| Stepper (TMC2209) | STEP 15, DIR 16, EN 17 (active LOW) |
| Barrel LED strip | GPIO18 (85 WS2812s = 5 rings of 17) |
| Fogger MOSFET | GPIO9 (active HIGH) |
| Vent LED strip (left) | GPIO41 (80 LEDs) |
| Vent LED strip (right) | GPIO40 (80 LEDs) |
| Status indicator | GPIO39 (2 LEDs, same type as the vents; index 0 = left) |

Reserved, do not use: GPIO19/20 (USB D-/D+), ~GPIO26-32 (OPI PSRAM),
GPIO0/3/45/46 (strapping pins).

## SD card contents

```
/idle.wav            stock sounds (required - the prop won't boot without them)
/fire.wav
/cooldown.wav        a single file containing 5 distinct "ka-chunk" hits
/AltMode1/           optional - single press of the mode button
    idle.wav         each of these four is optional (see below)
    fire.wav
    cooldown.wav
    mode.txt         this mode's colors / brightness / timing / volume
/AltMode2/           optional - double press (same four files)
/AltMode3/           optional - triple press (same four files)
```

Inside an AltMode folder, any missing WAV plays the stock one from the card
root instead, and a missing `mode.txt` means stock settings - so a folder
holding only a `mode.txt` is "stock sounds, different look/volume". A folder
with none of the four files (empty, or misnamed files) is rejected.

A commented template for `mode.txt` (listing every setting at its stock value)
is in `sd-card-template/mode.txt`. Copy it into each AltMode folder and edit.

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
whole barrel (and the status indicator) flashes the result: **green once**
(held ~0.5 s) = all checks passed (idle loop starts), **red twice** = a required subsystem failed and the firmware is
staying inert - the bars freeze where they stalled, so the unfilled ring shows
you which subsystem to look at. (A yellow-twice "degraded mode" code is
reserved for later.)

## Modes

The momentary button on **GPIO1** (wired just like the trigger) switches
between the stock setup and up to three alt modes stored on the SD card:

| Press | Mode |
|---|---|
| Single | `/AltMode1/` |
| Double | `/AltMode2/` |
| Triple | `/AltMode3/` |
| Long (hold ~1 s) | Stock - root sounds + built-in defaults, as if freshly booted |

Each alt mode has its own `idle.wav` / `fire.wav` / `cooldown.wav` and a
`mode.txt` with that mode's lighting, rainbow, volume and ka-chunk settings
(see `sd-card-template/mode.txt`). Every mode starts from stock and applies
its file on top. Any color can be the word `rainbow`: a rainbow idle color
holds a steady (non-breathing) chase, and a rainbow fire color makes the whole
barrel erupt into rainbow the moment the trigger is pulled. Switching modes
wipes the current sounds out of PSRAM and loads the new set from the card.

**Status indicator (GPIO39).** Normally mirrors the vents. When a mode is
requested it blacks out and blinks green: left LED = AltMode1, right =
AltMode2, both = AltMode3, left/right alternating = stock. If the mode is
borrowing any stock sounds from the card root, the same pattern turns
**turquoise** once it's loaded. Once the mode has loaded (and the pattern has
shown for at least 1.5 s) it goes back to mirroring the vents. If the alt mode
is unusable - folder empty or missing, or a bad line in `mode.txt` - it
flashes **red**, the switch is rejected, and the prop reverts to stock. The reason is printed on
the Serial console.

Pressing the button while firing or cooling down is fine: the indicator
starts blinking right away, and the switch happens as soon as the beam is
back at idle. Pressing the same mode again reloads it from the card (handy
after swapping SD cards). The debug console's `mode=stock|1|2|3` does the
same thing as the button.

## Debug console

Open the Serial Monitor at 115200 baud. Type `help` for the full list of
live-tunable settings (colors, timing, brightness, rainbow, volume). Every
key it accepts (except `mode=`) can also go in a `mode.txt`. Changes are
runtime-only - they reset on the next power cycle or mode switch.

## Troubleshooting

**Barrel won't spin up to full RPM *and* the speaker seems dead -> check the
12V trigger board first.** If both symptoms show up together, look at the
power-delivery trigger board's DIP switch before debugging the firmware or the
motor/amp. Its voltage-select switch has been seen to reset itself to **5V**
unintentionally, which starves both the stepper driver and the audio amp. Set
it back to **12V** and both problems go away. No firmware change is needed.

## Repo layout

- `SentinelBeam.ino` + the various `*Manager` classes - the actual firmware.
- `sd-card-template/mode.txt` - template settings file for the AltMode folders.
- `bring-up-tests/` - minimal standalone sketches used to test each subsystem
  on a freshly populated PCB, one at a time, independent of the main firmware.

## Versioning

See `CHANGELOG.md`. The running firmware version prints over Serial at boot.
