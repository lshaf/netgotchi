#include "Stats.h"
#include <SD.h>
#include <Arduino.h>
#include <M5Unified.h>

static constexpr const char* SAVE_PATH = "/netgotchi/stats";

void Stats::load() {
    File f = SD.open(SAVE_PATH, FILE_READ);
    if (!f) return;

    uint32_t xp = 0, captures = 0, cracked = 0;
    uint32_t deauthD = 0, floodD = 0, evilD = 0;
    f.read((uint8_t*)&xp,       4);
    f.read((uint8_t*)&captures, 4);
    f.read((uint8_t*)&cracked,  4);
    f.read((uint8_t*)&deauthD,  4);
    f.read((uint8_t*)&floodD,   4);
    f.read((uint8_t*)&evilD,    4);
    f.close();

    _xp              = xp;
    _captures        = captures;
    _cracked         = cracked;
    _deauthDiscovers = deauthD;
    _floodDiscovers  = floodD;
    _evilDiscovers   = evilD;
}

void Stats::save() const {
    File f = SD.open(SAVE_PATH, FILE_WRITE);
    if (!f) { Serial.println("[STATS] Save failed"); return; }

    f.write((const uint8_t*)&_xp,             4);
    f.write((const uint8_t*)&_captures,       4);
    f.write((const uint8_t*)&_cracked,        4);
    f.write((const uint8_t*)&_deauthDiscovers, 4);
    f.write((const uint8_t*)&_floodDiscovers,  4);
    f.write((const uint8_t*)&_evilDiscovers,   4);
    f.close();
}

int  Stats::battery()    const {
    int b = M5.Power.getBatteryLevel();
    return (b < 0) ? 0 : (b > 100 ? 100 : b);
}

bool Stats::isCharging() const { return M5.Power.isCharging(); }

void Stats::onCapture() {
    _captures++;
    _xp += XP_PER_CAPTURE;
    Serial.printf("[STATS] +%u XP  total_xp=%lu  caps=%lu\n",
                  XP_PER_CAPTURE, (unsigned long)_xp, (unsigned long)_captures);
}

void Stats::onCrack() {
    _cracked++;
    _xp += XP_PER_CRACK;
    Serial.printf("[STATS] +%u XP  total_xp=%lu  cracked=%lu\n",
                  XP_PER_CRACK, (unsigned long)_xp, (unsigned long)_cracked);
}

void Stats::onDeauthDiscover() {
    _deauthDiscovers++;
    _xp += XP_PER_DISCOVER;
}

void Stats::onFloodDiscover() {
    _floodDiscovers++;
    _xp += XP_PER_DISCOVER;
}

void Stats::onEvilDiscover() {
    _evilDiscovers++;
    _xp += XP_PER_DISCOVER;
}
