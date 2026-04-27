#include "Stats.h"
#include <SD.h>
#include <Arduino.h>

static constexpr const char* SAVE_PATH = "/netgotchi/stats";

void Stats::load() {
    File f = SD.open(SAVE_PATH, FILE_READ);
    if (!f) return;

    uint32_t magic = 0, xp = 0, captures = 0;
    f.read((uint8_t*)&magic,    4);
    f.read((uint8_t*)&xp,       4);
    f.read((uint8_t*)&captures, 4);
    f.close();

    if (magic != MAGIC) {
        Serial.println("[STATS] Bad magic, starting fresh");
        return;
    }

    _xp       = xp;
    _captures = captures;
    Serial.printf("[STATS] Loaded: xp=%lu caps=%lu\n",
                  (unsigned long)_xp, (unsigned long)_captures);
}

void Stats::save() const {
    File f = SD.open(SAVE_PATH, FILE_WRITE);
    if (!f) { Serial.println("[STATS] Save failed"); return; }

    uint32_t magic = MAGIC;
    f.write((const uint8_t*)&magic,     4);
    f.write((const uint8_t*)&_xp,       4);
    f.write((const uint8_t*)&_captures, 4);
    f.close();
}

void Stats::onCapture() {
    _captures++;
    _xp += XP_PER_CAPTURE;
    Serial.printf("[STATS] +%u XP  total_xp=%lu  caps=%lu\n",
                  XP_PER_CAPTURE, (unsigned long)_xp, (unsigned long)_captures);
}
