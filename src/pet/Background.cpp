#include "Background.h"
#include "../core/Palette.h"
#include "../core/types.h"
#include <M5Unified.h>

void Background::draw(M5Canvas& c) const {
    c.fillRect(0, BTN_STRIP, SCREEN_W, GROUND_Y - BTN_STRIP, Palette::SkyBot);

    // Stars — fixed pixel positions, two brightnesses
    static const struct { int8_t x; int8_t y; } STARS[] = {
        {10, 4}, {24, 2}, {38, 7}, {55, 3}, {68, 6}, {15, 9}, {50, 1},
    };
    for (auto& s : STARS) {
        int sy = BTN_STRIP + s.y;
        if (sy >= GROUND_Y) continue;
        c.drawPixel(s.x, sy, (&s - STARS) % 2 ? Palette::White : Palette::StarDim);
    }

    c.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, Palette::Ground);
    c.drawLine(0, GROUND_Y, SCREEN_W, GROUND_Y,            Palette::GroundEdge);
}
