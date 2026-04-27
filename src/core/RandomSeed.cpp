#include "RandomSeed.h"
#include <esp_system.h>
#include <esp_mac.h>
#include <Preferences.h>
#include <time.h>
#include <Arduino.h>

uint32_t RandomSeed::_prev = 0;

uint32_t RandomSeed::_build() {
    uint32_t seed = esp_random();
    seed ^= _prev;

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    for (int i = 0; i < 6; i++)
        seed ^= (uint32_t)mac[i] << ((i % 4) * 8);

    time_t now;
    time(&now);
    seed ^= (uint32_t)now;
    seed ^= (uint32_t)micros();

    _prev = seed;
    return seed;
}

void RandomSeed::init() {
    Preferences prefs;
    prefs.begin("netgotchi", false);
    _prev = prefs.getULong("rseed", 0);
    uint32_t seed = _build();
    prefs.putULong("rseed", seed);
    prefs.end();
    randomSeed(seed);
}

void RandomSeed::reseed() {
    randomSeed(_build());
}
