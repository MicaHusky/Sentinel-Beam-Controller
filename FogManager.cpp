#include "FogManager.h"
#include "Config.h"

void FogManager::begin() {
    pinMode(PIN_FOG, OUTPUT);
    digitalWrite(PIN_FOG, LOW); // off at boot
    Serial.println("[FogManager] initialized (OFF)");
}

void FogManager::startFiring() {
    digitalWrite(PIN_FOG, HIGH);
    Serial.println("[FogManager] ON");
}

void FogManager::stopFiring() {
    digitalWrite(PIN_FOG, LOW);
    Serial.println("[FogManager] OFF");
}
