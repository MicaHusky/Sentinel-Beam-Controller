#include "DebugConsole.h"

void DebugConsole::begin(LEDManager* led, AudioManager* audio, SettingsController* settings) {
    _led = led;
    _audio = audio;
    _settings = settings;
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

void DebugConsole::printHelp() {
    uint8_t ir, ig, ib, fr, fg, fb, vr, vg, vb;
    if (_led) {
        _led->getIdleColor(ir, ig, ib);
        _led->getFireColor(fr, fg, fb);
        _led->getVentColor(vr, vg, vb);
    }

    Serial.println(F("[Debug] ---- Sentinel Beam debug console ----"));
    Serial.println(F("[Debug] All changes are temporary - lost on next reboot."));
    Serial.println(F("[Debug] The phone web control page (connect to the 'Sentinel Beam Control'"));
    Serial.println(F("[Debug] WiFi network) offers the same settings with a graphical interface."));
    Serial.println(F("[Debug]"));
    Serial.println(F("[Debug]   idleColor=RRGGBB          LED color at idle"));
    Serial.println(F("[Debug]   fireColor=RRGGBB          LED color once fully wiped during firing"));
    Serial.println(F("[Debug]   ventColor=RRGGBB          vent strip color (same at idle and firing - only brightness changes)"));
    Serial.println(F("[Debug]   ventMaxBrightness=50      vent strip peak brightness during idle breathe, 0-100 (%)"));
    Serial.println(F("[Debug]   fireWipeSeconds=6.0       idle-color->fire-color fade duration while firing"));
    Serial.println(F("[Debug]   kachunk1..kachunk5=0.26   individually tune one of the 5 ka-chunk timestamps (sec, must stay ascending)"));
    Serial.println(F("[Debug]   kachunkTimestamps=t1,..,t5  set all 5 ka-chunk timestamps at once (sec, ascending)"));
    Serial.println(F("[Debug]   kachunkTail=0.30          extra time held after the last NEEDED ka-chunk before cooldown ends"));
    Serial.println(F("[Debug]   idleBreatheSeconds=3.0    one full idle breathe cycle"));
    Serial.println(F("[Debug]   idleMinBrightness=15      idle breathe low point, 0-100 (%)"));
    Serial.println(F("[Debug]   idleMaxBrightness=50      idle breathe high point, 0-100 (%)"));
    Serial.println(F("[Debug]   audioGain=0.15            playback gain, 0.0-2.0"));
    Serial.println(F("[Debug]   ledMode=rainbow|normal    override all LED states with a rainbow chase, or return to normal"));
    Serial.println(F("[Debug]   rainbowSpeed=40           chase speed, wheel positions/sec (higher = faster)"));
    Serial.println(F("[Debug]"));
    Serial.println(F("[Debug] Example: idleColor=33B5E5"));
    Serial.println(F("[Debug]"));
    Serial.println(F("[Debug] Current values:"));
    if (_led) {
        Serial.printf("[Debug]   idleColor=%s  fireColor=%s\n",
                      colorToHex(ir, ig, ib).c_str(), colorToHex(fr, fg, fb).c_str());
        Serial.printf("[Debug]   ventColor=%s  ventMaxBrightness=%.0f\n",
                      colorToHex(vr, vg, vb).c_str(), _led->getVentMaxBrightness() * 100.0f);
        Serial.printf("[Debug]   fireWipeSeconds=%.2f  idleBreatheSeconds=%.2f\n",
                      _led->getFireWipeSeconds(), _led->getIdleBreatheSeconds());
        Serial.printf("[Debug]   kachunkTimestamps=%.3f,%.3f,%.3f,%.3f,%.3f  kachunkTail=%.3f\n",
                      _led->getKachunkTimestamp(0), _led->getKachunkTimestamp(1), _led->getKachunkTimestamp(2),
                      _led->getKachunkTimestamp(3), _led->getKachunkTimestamp(4), _led->getKachunkTailSeconds());
        Serial.printf("[Debug]   idleMinBrightness=%.0f  idleMaxBrightness=%.0f\n",
                      _led->getIdleMinBrightness() * 100.0f, _led->getIdleMaxBrightness() * 100.0f);
        Serial.printf("[Debug]   ledMode=%s  rainbowSpeed=%.1f\n",
                      _led->isRainbowMode() ? "rainbow" : "normal", _led->getRainbowSpeed());
    }
    if (_audio) {
        Serial.printf("[Debug]   audioGain=%.2f\n", _audio->getGain());
    }
}
