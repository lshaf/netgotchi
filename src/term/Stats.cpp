#include "Stats.h"
#include "../hw/Hw.h"
#include <SD.h>
#include <Arduino.h>

static constexpr const char* SAVE_PATH = "/netgotchi/stats";

void Stats::load() {
    File f = Hw::sd.open(SAVE_PATH, FILE_READ);
    if (!f) return;

    uint32_t xp = 0, captures = 0, cracked = 0, discovers = 0;
    f.read((uint8_t*)&xp,        4);
    f.read((uint8_t*)&captures,  4);
    f.read((uint8_t*)&cracked,   4);
    f.read((uint8_t*)&discovers, 4);
    f.close();

    _xp        = xp;
    _captures  = captures;
    _cracked   = cracked;
    _discovers = discovers;
}

void Stats::save() const {
    File f = Hw::sd.open(SAVE_PATH, FILE_WRITE);
    if (!f) return;

    f.write((const uint8_t*)&_xp,        4);
    f.write((const uint8_t*)&_captures,  4);
    f.write((const uint8_t*)&_cracked,   4);
    f.write((const uint8_t*)&_discovers, 4);
    f.close();
}

int  Stats::battery()    const {
    int b = (int)Hw::axp.getBatteryLevel();
    return (b < 0) ? 0 : (b > 100 ? 100 : b);
}
bool Stats::isCharging() const { return Hw::axp.isCharging(); }

void Stats::onCapture() {
    _captures++;
    _xp += XP_PER_CAPTURE;
}

void Stats::onCrack() {
    _cracked++;
    _xp += XP_PER_CRACK;
}

void Stats::onDiscover() {
    _discovers++;
    _xp += XP_PER_DISCOVER;
}
