#include "PetStats.h"
#include <SD.h>
#include <Arduino.h>

const PetStats::Achievement PetStats::ACHIEVEMENTS[PetStats::ACH_N] = {
    {  1, "1st sniff!" },
    {  5, "hacker~"    },
    { 10, "elite hax"  },
    { 25, "ghost mode" },
    { 50, "wifi god~"  },
};

void PetStats::load() {
    File f = SD.open("/netgotchi/pet", FILE_READ);
    if (!f) return;

    uint32_t magic = 0, caps = 0, exp = 0;
    uint8_t  ach = 0;
    f.read((uint8_t*)&magic, 4);
    f.read((uint8_t*)&caps,  4);
    f.read((uint8_t*)&exp,   4);
    f.read((uint8_t*)&ach,   1);
    f.close();

    if (magic != MAGIC) {
        Serial.println("[PET] Bad save magic, starting fresh");
        return;
    }

    _totalCaptures = caps;
    _exp           = exp;
    _nextAch       = ach;
    uint32_t lv    = 1 + _exp / 50;
    _level         = (lv > 99) ? 99 : (uint8_t)lv;

    Serial.printf("[PET] Loaded: caps=%lu exp=%lu lvl=%d\n",
                  (unsigned long)_totalCaptures, (unsigned long)_exp, _level);
}

void PetStats::save() const {
    File f = SD.open("/netgotchi/pet", FILE_WRITE);
    if (!f) { Serial.println("[PET] Save failed"); return; }

    uint32_t magic = MAGIC;
    f.write((const uint8_t*)&magic,          4);
    f.write((const uint8_t*)&_totalCaptures, 4);
    f.write((const uint8_t*)&_exp,           4);
    f.write((const uint8_t*)&_nextAch,       1);
    f.close();

    Serial.printf("[PET] Saved: caps=%lu exp=%lu lvl=%d\n",
                  (unsigned long)_totalCaptures, (unsigned long)_exp, _level);
}

bool PetStats::onCapture() {
    _totalCaptures++;

    uint32_t gain = 20 + (uint32_t)_level * 5;
    _exp         += gain;

    uint8_t  prevLevel = _level;
    uint32_t lv        = 1 + _exp / 50;
    _level             = (lv > 99) ? 99 : (uint8_t)lv;
    _leveledUp         = (_level > prevLevel);

    Serial.printf("[EXP] +%lu  total=%lu  lvl=%d\n",
                  (unsigned long)gain, (unsigned long)_exp, _level);

    _achMsg = nullptr;
    if (!_leveledUp && _nextAch < ACH_N && _totalCaptures >= ACHIEVEMENTS[_nextAch].threshold) {
        _achMsg = ACHIEVEMENTS[_nextAch].msg;
        _nextAch++;
        Serial.printf("[ACHIEV] \"%s\" at %lu captures\n",
                      _achMsg, (unsigned long)_totalCaptures);
    }

    return _leveledUp;
}
