#include "App.h"
#include "AppLayout.h"
#include "Virus.h"
#include "Theme.h"
#include "command/CrackCommand.h"
#include <M5Unified.h>
#include <esp_heap_caps.h>

using namespace AppLayout;

extern CrackCommand s_crack;
extern MenuCommand* s_rootItems[];
extern int          ROOT_N;

// ── Header bar ────────────────────────────────────────────────

static void drawValueBar(M5Canvas& c,
                         int barX, int barY, int barW, int barH,
                         const char* label, const char* valText, int fillPct)
{
    if (barW < 8 || barH < 6) return;
    if (fillPct < 0)   fillPct = 0;
    if (fillPct > 100) fillPct = 100;

    c.fillRect(barX, barY, barW, barH, Theme::PALE);
    c.drawRect(barX, barY, barW, barH, Theme::DIM);

    const int innerW = barW - 2;
    const int innerH = barH - 2;
    const int innerX = barX + 1;
    const int innerY = barY + 1;
    const int fill   = innerW * fillPct / 100;
    if (fill > 0) c.fillRect(innerX, innerY, fill, innerH, Theme::FG);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Theme::BG);
    const int midY = barY + barH / 2 + 1;
    const int padL = (barH - CHAR_H) / 2;

    c.setTextDatum(lgfx::middle_left);
    c.drawString(label, barX + padL + 1, midY);

    c.setTextDatum(lgfx::middle_right);
    c.drawString(valText, barX + barW - padL + 1, midY);
}

void App::_drawHud(M5Canvas& c, uint32_t ms) const {
    c.fillRect(0, 0, SCR_W, HEAD_H, Theme::BG);

    int  bat      = _stats.battery();
    bool charging = _stats.isCharging();

    uint32_t freeH  = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
                    + (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t totalH = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_INTERNAL)
                    + (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    int ram = (totalH > 0) ? (int)((uint64_t)(totalH - freeH) * 100u / totalH) : 0;
    if (ram > 100) ram = 100;
    if (ram < 0)   ram = 0;

    const int stripW = CELL_RIGHT - MARGIN;
    const int eachW  = (stripW - BAR_GAP) / 2;
    const int batX   = MARGIN;
    const int ramX   = batX + eachW + BAR_GAP;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", bat);
    drawValueBar(c, batX, BAR1_Y, eachW, BAR_H, charging ? "BAT++" : "BAT", buf, bat);

    snprintf(buf, sizeof(buf), "%d%%", ram);
    drawValueBar(c, ramX, BAR1_Y, eachW, BAR_H, "RAM", buf, ram);

    snprintf(buf, sizeof(buf), "LV%lu", (unsigned long)_stats.level());
    drawValueBar(c, MARGIN, BAR2_Y, stripW, BAR_H, "EXP", buf, (int)_stats.xpProgress());

    c.drawFastHLine(0, HEADER_DIVIDER_Y, SCR_W, Theme::DIM);

    Virus::State vs;
    if      (s_crack.isRunning())                              vs = Virus::State::Decrypting;
    else if (_nethuntRunning && _exhaustPhase != 0)            vs = Virus::State::Sleep;
    else if (_nethuntRunning)                                  vs = Virus::State::Active;
    else if (_nettrapRunning)                                  vs = Virus::State::Trap;
    else if (_netguardRunning)                                 vs = Virus::State::Guard;
    else                                                       vs = Virus::State::Idle;
    Virus::draw(c, ms, vs);
}

// ── Scrollback ────────────────────────────────────────────────

void App::_drawLog(M5Canvas& c) const {
    c.fillRect(0, LOG_TOP, SCR_W, LOG_BOT - LOG_TOP, Theme::BG);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Theme::FG, Theme::BG);
    c.setTextDatum(lgfx::top_left);

    const int maxVis    = (LOG_BOT - LOG_TOP) / LINE_H;
    const bool cracking = s_crack.isRunning();
    int        scrollSlot = 0;

    if (cracking) {
        uint32_t pct = (s_crack.fileSize() > 0)
            ? (uint32_t)((uint64_t)s_crack.bytesDone() * 100 / s_crack.fileSize())
            : 0;
        if (pct > 100) pct = 100;
        char bar[21]; int filled = (int)(20 * pct / 100);
        for (int i = 0; i < 20; i++) bar[i] = (i < filled) ? '#' : ' ';
        bar[20] = '\0';

        char speedBuf[10] = "";
        char etaBuf[10]   = "";
        uint32_t elapsed_s = (millis() - s_crack.startMs() + 500) / 1000;
        if (elapsed_s > 0) {
            uint32_t wps = s_crack.tested() / elapsed_s;
            if (wps >= 1000) snprintf(speedBuf, sizeof(speedBuf), " %luk/s", (unsigned long)(wps / 1000));
            else             snprintf(speedBuf, sizeof(speedBuf), " %lu/s",  (unsigned long)wps);

            uint32_t bps = (s_crack.bytesDone() > 0) ? s_crack.bytesDone() / elapsed_s : 0;
            if (bps > 0 && s_crack.fileSize() > s_crack.bytesDone()) {
                uint32_t eta = (s_crack.fileSize() - s_crack.bytesDone()) / bps;
                if (eta < 60) snprintf(etaBuf, sizeof(etaBuf), " %lus",      (unsigned long)eta);
                else          snprintf(etaBuf, sizeof(etaBuf), " %lum%02lus", (unsigned long)(eta / 60), (unsigned long)(eta % 60));
            }
        }

        char buf[LINE_COL];
        snprintf(buf, sizeof(buf), "[%s] %lu%%%s%s", bar, (unsigned long)pct, speedBuf, etaBuf);
        int y = LOG_BOT - LINE_H + 1;
        c.drawString(buf, MARGIN, y);
        scrollSlot = 1;
    }

    for (int i = 0; i + scrollSlot < maxVis && i < LOG_LINES; i++) {
        int idx = (_logHead - 1 - i + LOG_LINES * 4) % LOG_LINES;
        if (_logBuf[idx][0] == '\0') break;
        int y = LOG_BOT - LINE_H * (i + 1 + scrollSlot) + 1;
        c.drawString(_logBuf[idx], MARGIN, y);
    }
}

// ── Input prompt ──────────────────────────────────────────────

void App::_drawInput(M5Canvas& c) const {
    c.drawFastHLine(0, INPUT_DIVIDER_Y, SCR_W, Theme::DIM);
    c.fillRect(0, INPUT_DIVIDER_Y + 1, SCR_W, SCR_H - (INPUT_DIVIDER_Y + 1), Theme::BG);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Theme::FG, Theme::BG);
    c.setTextDatum(lgfx::top_left);

    c.drawString("$ ", MARGIN, INPUT_Y);

    char buf[LINE_COL];
    int n = _typeIdx;
    if (n > LINE_COL - 1) n = LINE_COL - 1;
    memcpy(buf, _typeLine, n);
    buf[n] = '\0';
    const int textX = MARGIN + 2 * CHAR_W;
    c.drawString(buf, textX, INPUT_Y);

    bool show = true;
    if (_typeLen == 0 || _typeDone) show = _cursorOn;
    if (show) {
        int cx = textX + n * CHAR_W;
        c.fillRect(cx, INPUT_Y, CHAR_W - 1, CHAR_H, Theme::FG);
    }
}

// ── Menu overlay ──────────────────────────────────────────────

void App::_drawMenuContent(M5Canvas& c) const {
    c.setFont(&fonts::Font0);
    c.setTextSize(1);

    int nItems = 0, itemH = MENU_ITEM_H;
    if (_menuState == MenuState::Root) {
        nItems = (_nethuntRunning || _nettrapRunning || _netguardRunning || s_crack.isRunning()) ? 1 : ROOT_N;
    } else if (_menuState == MenuState::Sub && _activeSubCmd) {
        nItems = _activeSubCmd->subCount();
        itemH  = _activeSubCmd->subItemH();
    }

    int maxVis = (INPUT_DIVIDER_Y - HEADER_DIVIDER_Y - 4) / itemH;
    if (maxVis < 3) maxVis = 3;
    bool paginated     = nItems > maxVis;
    int  slotCount     = paginated ? maxVis     : nItems;
    int  itemsPerPage  = paginated ? maxVis - 2 : nItems;
    int  firstItemSlot = paginated ? 1          : 0;
    int  scroll        = _menuScroll;
    if (paginated) {
        int maxScroll = nItems - itemsPerPage;
        if (scroll < 0)         scroll = 0;
        if (scroll > maxScroll) scroll = maxScroll;
    } else {
        scroll = 0;
    }
    int menuTop = INPUT_DIVIDER_Y - slotCount * itemH;

    c.fillRect(0, menuTop, SCR_W, INPUT_DIVIDER_Y - menuTop, Theme::BG);
    c.drawFastHLine(0, menuTop, SCR_W, Theme::DIM);

    auto drawItem = [&](int slot, const char* label, bool active) {
        int y   = menuTop + slot * itemH;
        bool hi = ((int)_menuHighlight == slot);
        uint16_t col = active ? Theme::FG : Theme::DIM;
        if (hi) c.fillRect(0, y, SCR_W, itemH, Theme::PALE);
        c.setTextColor(col, hi ? Theme::PALE : Theme::BG);
        c.setTextDatum(lgfx::middle_left);
        c.drawString(label, MARGIN + 6, y + itemH / 2);
    };

    auto drawNav = [&](int slot, const char* label, bool enabled) {
        int y   = menuTop + slot * itemH;
        bool hi = ((int)_menuHighlight == slot);
        if (hi) c.fillRect(0, y, SCR_W, itemH, Theme::PALE);
        c.setTextColor(enabled ? Theme::FG : Theme::DIM, hi ? Theme::PALE : Theme::BG);
        c.setTextDatum(lgfx::middle_center);
        c.drawString(label, SCR_W / 2, y + itemH / 2);
    };

    for (int slot = 0; slot < slotCount; slot++) {
        if (paginated && slot == 0) {
            drawNav(slot, "<< prev", scroll > 0);
            continue;
        }
        if (paginated && slot == slotCount - 1) {
            bool more = (scroll + itemsPerPage) < nItems;
            drawNav(slot, "next >>", more);
            continue;
        }

        int itemIdx = scroll + (slot - firstItemSlot);
        if (itemIdx < 0 || itemIdx >= nItems) continue;

        if (_menuState == MenuState::Root) {
            const bool locked = _nethuntRunning || _nettrapRunning || _netguardRunning || s_crack.isRunning();
            const char* lbl = locked ? "stop" : s_rootItems[itemIdx]->label();
            drawItem(slot, lbl, true);
        } else if (_menuState == MenuState::Sub && _activeSubCmd) {
            bool act = _activeSubCmd->subIsActive(itemIdx);
            bool lit = !_activeSubCmd->subUseDim() || act;
            drawItem(slot, _activeSubCmd->subLabel(itemIdx), lit);
            if (act) {
                int y = menuTop + slot * itemH;
                c.setTextDatum(lgfx::middle_right);
                c.drawString("*", SCR_W - MARGIN - 6, y + itemH / 2);
            }
        }
    }

    c.drawFastHLine(0, INPUT_DIVIDER_Y, SCR_W, Theme::DIM);
    c.fillRect(0, INPUT_DIVIDER_Y + 1, SCR_W, SCR_H - (INPUT_DIVIDER_Y + 1), Theme::BG);
    c.setTextColor(Theme::FG, Theme::BG);
    c.setTextDatum(lgfx::top_left);
    c.drawString("$ ", MARGIN, INPUT_Y);

    const char* inputText = (_menuState == MenuState::Sub && _activeSubCmd)
                            ? _activeSubCmd->inputHint() : "";
    const int textX = MARGIN + 2 * CHAR_W;
    if (inputText[0]) c.drawString(inputText, textX, INPUT_Y);
    if (_cursorOn) {
        int cx = textX + (int)strlen(inputText) * CHAR_W;
        c.fillRect(cx, INPUT_Y, CHAR_W - 1, CHAR_H, Theme::FG);
    }
}
