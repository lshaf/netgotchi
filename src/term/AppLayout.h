#pragma once
#include "Virus.h"
#include <cstdint>

namespace AppLayout {
    constexpr int SCR_W   = 320;
    constexpr int SCR_H   = 240;

    constexpr int MARGIN  = 4;
    constexpr int BAR_H   = 15;
    constexpr int BAR_GAP = 2;

    constexpr int HEAD_TOP_PAD     = 4;
    constexpr int HEAD_BOT_PAD     = 2;
    constexpr int BAR1_Y           = HEAD_TOP_PAD;
    constexpr int BAR2_Y           = BAR1_Y + BAR_H + BAR_GAP;
    constexpr int HEAD_H           = BAR2_Y + BAR_H + HEAD_BOT_PAD;
    constexpr int DISP_BTN_X = MARGIN;                              // 4
    constexpr int DISP_BTN_Y = Virus::Y0;                           // 4 — matches mascot Y
    constexpr int DISP_BTN_W = Virus::SIDE;                        // 32 — same as mascot
    constexpr int DISP_BTN_H = Virus::SIDE;                        // 32 — same as mascot
    constexpr int BAR_LEFT   = DISP_BTN_X + DISP_BTN_W + MARGIN;  // 40
    constexpr int CELL_RIGHT       = Virus::X0 - MARGIN;

    constexpr int HEADER_DIVIDER_Y = HEAD_H;
    constexpr int LOG_TOP          = HEADER_DIVIDER_Y + 1 + 4;
    constexpr int INPUT_DIVIDER_Y  = SCR_H - 22;
    constexpr int LOG_BOT          = INPUT_DIVIDER_Y - 4;
    constexpr int INPUT_Y          = INPUT_DIVIDER_Y + 1 + 5;

    constexpr int LINE_H  = 9;
    constexpr int CHAR_W  = 6;
    constexpr int CHAR_H  = 8;
    constexpr int LOG_MAXW = (SCR_W - 2 * MARGIN) / CHAR_W;
    constexpr int LOG_BODY = LOG_MAXW - 2;

    constexpr uint32_t TYPE_STEP_MS = 15;
    constexpr uint32_t TYPE_HOLD_MS = 400;
    constexpr uint32_t CURSOR_MS    = 480;

    constexpr int MENU_ITEM_H = 18;
    constexpr int BRIGHT_BH   = 14;
}
