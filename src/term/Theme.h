#pragma once
#include <cstdint>

class Theme {
public:
    struct Entry {
        uint16_t bg, fg, pale, dim;
        const char* name;
    };

    static constexpr int COUNT = 4;
    static const Entry   ENTRIES[COUNT];

    static void apply(int idx);
    static void applyBrightness(uint8_t b);
    static void load();
    static void save();

    static int     idx()        { return _idx; }
    static uint8_t brightness() { return _brightness; }

    // Current palette — mutated by apply(); read directly by draw code
    static uint16_t BG, FG, PALE, DIM;

private:
    static int     _idx;
    static uint8_t _brightness;
};
