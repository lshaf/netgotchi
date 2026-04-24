#include "Background.h"
#include "../core/Palette.h"
#include "../core/types.h"
#include <M5Unified.h>

void Background::draw(M5Canvas& c) const {
    c.fillRect(0, BTN_STRIP, SCREEN_W, GROUND_Y - BTN_STRIP, Palette::SkyBot);

    // Stars spread across the 320×240 sky
    static const struct { int16_t x; int16_t y; } STARS[] = {
        { 40, 16}, { 96,  8}, {152, 28}, {220, 12},
        {272, 24}, { 60, 36}, {200,  4}, {130, 20},
        {300, 14}, { 20, 32},
    };
    for (auto& s : STARS) {
        int sy = BTN_STRIP + s.y;
        if (sy >= GROUND_Y) continue;
        uint16_t col = (&s - STARS) % 2 ? Palette::White : Palette::StarDim;
        c.fillRect(s.x, sy, 2, 2, col);
    }

    c.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, Palette::Ground);
    c.drawLine(0, GROUND_Y, SCREEN_W, GROUND_Y,            Palette::GroundEdge);
}
