#include "DebugConsole.h"

void DebugConsole::begin(LEDManager* led, AudioManager* audio, SettingsController* settings, ModeManager* mode,
                         DisplayModeManager* display) {
    _display = display;
    _led = led;
    _audio = audio;
    _settings = settings;
    _mode = mode;
    _lineBuffer = "";
    Serial.println("[Debug] Serial console ready - type 'help' for commands.");
}

void DebugConsole::update() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (_lineBuffer.length() > 0) {
                processLine(_lineBuffer);
                _lineBuffer = "";
            }
        } else {
            _lineBuffer += c;
        }
    }
}

String DebugConsole::colorToHex(uint8_t r, uint8_t g, uint8_t b) {
    char buf[7];
    snprintf(buf, sizeof(buf), "%02X%02X%02X", r, g, b);
    return String(buf);
}

void DebugConsole::processLine(String line) {
    line.trim();
    if (line.length() == 0) return;

    if (line.equalsIgnoreCase("help")) {
        printHelp();
        return;
    }

    int eq = line.indexOf('=');
    if (eq < 0) {
        Serial.printf("[Debug] ERROR: unrecognized command '%s'. Type 'help' for a list of commands.\n",
                      line.c_str());
        return;
    }

    String key = line.substring(0, eq);
    String value = line.substring(eq + 1);

    if (_settings) {
        SettingResult result = _settings->apply(key, value);
        Serial.printf("[Debug] %s\n", result.message.c_str());
    }
}

String DebugConsole::colorToString(const SlotColor& c) {
    return c.rainbow ? String("rainbow") : colorToHex(c.r, c.g, c.b);
}

void DebugConsole::printHelp() {
    Serial.println(F("[Debug] ---- Sentinel Beam debug console ----"));
    Serial.println(F("[Debug] All changes are temporary - lost on reboot or the next mode switch."));
    Serial.println(F("[Debug] Every key below (except mode=) can also go in a mode.txt / DisplayMode.txt."));
    Serial.println(F("[Debug] Colors take 6 hex digits (optional #) or the word rainbow."));
    Serial.println(F("[Debug]"));
    Serial.println(F("[Debug]   mode=stock|1|2|3|display  switch mode, same as the GPIO1 button (queued until idle)"));
    Serial.println(F("[Debug]   idleColor=RRGGBB          barrel color at idle"));
    Serial.println(F("[Debug]   fireColor=RRGGBB          barrel color once fully wiped during firing (rainbow = instant, no wipe)"));
    Serial.println(F("[Debug]   ventColor=RRGGBB          vent color at idle"));
    Serial.println(F("[Debug]   ventFireColor=RRGGBB      vent color while firing + kachunk cooldown"));
    Serial.println(F("[Debug]   ventMaxBrightness=50      vent strip peak brightness during idle breathe, 0-100 (%)"));
    Serial.println(F("[Debug]   fireBrightness=100        barrel brightness while firing + ka-chunks, 0-100 (%)"));
    Serial.println(F("[Debug]   ventFireBrightness=100    vent brightness while firing + ka-chunks, 0-100 (%)"));
    Serial.println(F("[Debug]   fireWipeSeconds=6.0       idle-color->fire-color fade duration while firing"));
    Serial.println(F("[Debug]   kachunk1..kachunk5=0.26   individually tune one of the 5 ka-chunk timestamps (sec, must stay ascending)"));
    Serial.println(F("[Debug]   kachunkTimestamps=t1,..,t5  set all 5 ka-chunk timestamps at once (sec, ascending)"));
    Serial.println(F("[Debug]   kachunkTail=0.30          extra time held after the last NEEDED ka-chunk before cooldown ends"));
    Serial.println(F("[Debug]   idleBreatheSeconds=3.0    one full idle breathe cycle"));
    Serial.println(F("[Debug]   idleMinBrightness=15      idle breathe low point, 0-100 (%)"));
    Serial.println(F("[Debug]   idleMaxBrightness=50      idle breathe high point, 0-100 (%)"));
    Serial.println(F("[Debug]   rainbowSpeed=40           rainbow chase speed, wheel positions/sec (higher = faster)"));
    Serial.println(F("[Debug]   rainbowBrightness=100     fixed brightness of any rainbow color, 0-100 (%) - never breathes"));
    Serial.println(F("[Debug]   audioGain=0.15            playback gain, 0.0-2.0"));
    Serial.println(F("[Debug]   longPressMs=1000          mode button hold time for a long press (-> stock)"));
    Serial.println(F("[Debug]   multiPressGapMs=350       max gap between taps of a double/triple press"));
    Serial.println(F("[Debug]   displayExitHoldMs=5000    hold time to leave display mode (-> stock)"));
    Serial.println(F("[Debug]  display mode (ranges: either order is fine):"));
    Serial.println(F("[Debug]   activityMinSeconds=20 / activityMaxSeconds=60     random gap between activity.wav plays"));
    Serial.println(F("[Debug]   twitchCountMin=1 / twitchCountMax=5               twitches per activity (1-10)"));
    Serial.println(F("[Debug]   twitchMinSeconds=0.3 / twitchMaxSeconds=1.5       how long each twitch drives the barrel"));
    Serial.println(F("[Debug]   twitchPauseMinSeconds=0.2 / twitchPauseMaxSeconds=1.0  rest between twitches"));
    Serial.println(F("[Debug]   twitchRPM=10              twitch speed (max 30; firing is ~70)"));
    Serial.println(F("[Debug]   twitchAccelPercent=50     twitch accel/decel as % of the firing accel"));
    Serial.println(F("[Debug]"));
    Serial.println(F("[Debug] Example: idleColor=33B5E5"));
    Serial.println(F("[Debug]"));
    Serial.println(F("[Debug] Current values:"));
    if (_mode) {
        Serial.printf("[Debug]   mode=%s%s  longPressMs=%u  multiPressGapMs=%u\n",
                      profileName(_mode->current()),
                      _mode->hasPending() ? " (switch pending)" : "",
                      (unsigned)_mode->getLongPressMs(), (unsigned)_mode->getMultiPressGapMs());
    }
    if (_led) {
        Serial.printf("[Debug]   idleColor=%s  fireColor=%s\n",
                      colorToString(_led->getColor(ColorSlot::IDLE)).c_str(),
                      colorToString(_led->getColor(ColorSlot::FIRE)).c_str());
        Serial.printf("[Debug]   ventColor=%s  ventFireColor=%s  ventMaxBrightness=%.0f\n",
                      colorToString(_led->getColor(ColorSlot::VENT)).c_str(),
                      colorToString(_led->getColor(ColorSlot::VENT_FIRE)).c_str(),
                      _led->getVentMaxBrightness() * 100.0f);
        Serial.printf("[Debug]   fireWipeSeconds=%.2f  idleBreatheSeconds=%.2f\n",
                      _led->getFireWipeSeconds(), _led->getIdleBreatheSeconds());
        Serial.printf("[Debug]   kachunkTimestamps=%.3f,%.3f,%.3f,%.3f,%.3f  kachunkTail=%.3f\n",
                      _led->getKachunkTimestamp(0), _led->getKachunkTimestamp(1), _led->getKachunkTimestamp(2),
                      _led->getKachunkTimestamp(3), _led->getKachunkTimestamp(4), _led->getKachunkTailSeconds());
        Serial.printf("[Debug]   idleMinBrightness=%.0f  idleMaxBrightness=%.0f\n",
                      _led->getIdleMinBrightness() * 100.0f, _led->getIdleMaxBrightness() * 100.0f);
        Serial.printf("[Debug]   fireBrightness=%.0f  ventFireBrightness=%.0f\n",
                      _led->getFireBrightness() * 100.0f, _led->getVentFireBrightness() * 100.0f);
        Serial.printf("[Debug]   rainbowSpeed=%.1f  rainbowBrightness=%.0f\n",
                      _led->getRainbowSpeed(), _led->getRainbowBrightness() * 100.0f);
    }
    if (_audio) {
        Serial.printf("[Debug]   audioGain=%.2f\n", _audio->getGain());
    }
    if (_display) {
        Serial.printf("[Debug]   displayExitHoldMs=%u  activity=%.1f-%.1f s  twitchCount=%u-%u\n",
                      _mode ? (unsigned)_mode->getDisplayExitHoldMs() : 0u,
                      _display->activityMinSeconds, _display->activityMaxSeconds,
                      _display->twitchCountMin, _display->twitchCountMax);
        Serial.printf("[Debug]   twitch=%.2f-%.2f s  pause=%.2f-%.2f s  twitchRPM=%.1f  twitchAccelPercent=%.0f\n",
                      _display->twitchMinSeconds, _display->twitchMaxSeconds,
                      _display->twitchPauseMinSeconds, _display->twitchPauseMaxSeconds,
                      _display->twitchRPM, _display->twitchAccelPercent);
    }
}
