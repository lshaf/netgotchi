#include "Theme.h"
#include <M5Unified.h>
#include <SD.h>
#include <Arduino.h>

static constexpr uint16_t rgb16(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3);
}
static constexpr uint32_t    CONFIG_MAGIC_V1 = 0xCF191700;
static constexpr uint32_t    CONFIG_MAGIC    = 0xCF191701;
static constexpr const char* CONFIG_PATH     = "/netgotchi/config";

// ── Static member definitions ─────────────────────────────────

const Theme::Entry Theme::ENTRIES[Theme::COUNT] = {
    { rgb16(  0,  0,  0), rgb16(  0,220, 60), rgb16(  0, 90, 28), rgb16(  0, 70, 20), "green"  },
    { rgb16(  0,  0,  0), rgb16(255,176,  0), rgb16(100, 60,  0), rgb16( 70, 42,  0), "amber"  },
    { rgb16(  0,  0,  0), rgb16(  0,210,220), rgb16(  0, 80, 90), rgb16(  0, 55, 65), "cyan"   },
    { rgb16(  0,  0,  0), rgb16(220, 50, 50), rgb16( 90, 18, 18), rgb16( 65, 12, 12), "red"    },
    { rgb16(  0,  0,  0), rgb16( 80,160,255), rgb16( 20, 55,110), rgb16( 12, 38, 80), "blue"   },
    { rgb16(  0,  0,  0), rgb16(190, 80,255), rgb16( 70, 20,110), rgb16( 50, 12, 80), "purple" },
    { rgb16(  0,  0,  0), rgb16(255,100,180), rgb16(110, 30, 75), rgb16( 80, 18, 55), "pink"   },
    { rgb16(  0,  0,  0), rgb16(200,200,200), rgb16( 70, 70, 70), rgb16( 45, 45, 45), "white"  },
};

int      Theme::_idx            = 0;
uint8_t  Theme::_brightness     = 200;
uint16_t Theme::_displayOffSecs = 0;

uint16_t Theme::BG   = Theme::ENTRIES[0].bg;
uint16_t Theme::FG   = Theme::ENTRIES[0].fg;
uint16_t Theme::PALE = Theme::ENTRIES[0].pale;
uint16_t Theme::DIM  = Theme::ENTRIES[0].dim;

// ── Methods ───────────────────────────────────────────────────

void Theme::apply(int idx) {
    _idx = ((idx % COUNT) + COUNT) % COUNT;
    const Entry& e = ENTRIES[_idx];
    BG   = e.bg;
    FG   = e.fg;
    PALE = e.pale;
    DIM  = e.dim;
    save();
}

void Theme::applyBrightness(uint8_t b) {
    _brightness = b;
    M5.Display.setBrightness(b);
    save();
}

void Theme::applyDisplayOff(uint16_t secs) {
    _displayOffSecs = secs;
    save();
}

void Theme::save() {
    File f = SD.open(CONFIG_PATH, FILE_WRITE);
    if (!f) { Serial.println("[CFG] Save failed"); return; }
    uint32_t magic = CONFIG_MAGIC;
    uint8_t  theme = (uint8_t)_idx;
    f.write((const uint8_t*)&magic,          4);
    f.write(&theme,                          1);
    f.write(&_brightness,                    1);
    f.write((const uint8_t*)&_displayOffSecs, 2);
    f.close();
}

void Theme::load() {
    File f = SD.open(CONFIG_PATH, FILE_READ);
    if (!f) return;

    uint32_t magic = 0;
    uint8_t  theme = 0, bright = 200;
    uint16_t dispOff = 0;
    f.read((uint8_t*)&magic, 4);
    f.read(&theme,  1);
    f.read(&bright, 1);

    if (magic == CONFIG_MAGIC) {
        f.read((uint8_t*)&dispOff, 2);
    } else if (magic != CONFIG_MAGIC_V1) {
        f.close();
        Serial.println("[CFG] Bad magic, using defaults");
        return;
    }
    f.close();

    _idx            = theme % COUNT;
    _brightness     = bright;
    _displayOffSecs = dispOff;

    const Entry& e = ENTRIES[_idx];
    BG   = e.bg;
    FG   = e.fg;
    PALE = e.pale;
    DIM  = e.dim;
    M5.Display.setBrightness(bright);

    Serial.printf("[CFG] Loaded: theme=%d bright=%d dispoff=%ds\n", _idx, bright, dispOff);
}
