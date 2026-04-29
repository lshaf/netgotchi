#include "App.h"
#include "AppLayout.h"
#include "Virus.h"
#include "Theme.h"
#include <M5Unified.h>
#include <esp_heap_caps.h>

using namespace AppLayout;

extern MenuCommand*  s_rootItems[];
extern int           ROOT_N;

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

    Virus::State vs = _currentService ? _currentService->virusState() : Virus::State::Idle;
    Virus::draw(c, ms, vs);
}

// ── Scrollback ────────────────────────────────────────────────

void App::_drawLog(M5Canvas& c) const {
    c.fillRect(0, LOG_TOP, SCR_W, LOG_BOT - LOG_TOP, Theme::BG);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Theme::FG, Theme::BG);
    c.setTextDatum(lgfx::top_left);

    const int maxVis  = (LOG_BOT - LOG_TOP) / LINE_H;
    int       scrollSlot = 0;

    if (_currentService) {
        char buf[LINE_COL];
        if (_currentService->progressLine(buf, LINE_COL, millis())) {
            c.drawString(buf, MARGIN, LOG_BOT - LINE_H + 1);
            scrollSlot = 1;
        }
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
        nItems = _currentService ? 1 : ROOT_N;
    } else if (_menuState == MenuState::Sub && _activeSubCmd) {
        nItems = _activeSubCmd->subCount();
        itemH  = _activeSubCmd->subItemH();
    }

    int maxVis = (INPUT_DIVIDER_Y - HEADER_DIVIDER_Y - 4) / itemH;
    if (maxVis < 3) maxVis = 3;
    bool paginated     = nItems > maxVis;
    int  slotCount     = paginated ? maxVis + 1 : nItems;
    int  itemsPerPage  = paginated ? maxVis     : nItems;
    int  firstItemSlot = paginated ? 1 : 0;
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
        c.drawString(label, MARGIN + 6, y + itemH / 2 + 1);
    };

    for (int slot = 0; slot < slotCount; slot++) {
        if (paginated && slot == 0) {
            bool hasPrev = scroll > 0;
            bool hasNext = (scroll + itemsPerPage) < nItems;
            int y = menuTop + slot * itemH;
            if (_navHighlight == 0) c.fillRect(0,         y, SCR_W / 2, itemH, Theme::PALE);
            if (_navHighlight == 1) c.fillRect(SCR_W / 2, y, SCR_W / 2, itemH, Theme::PALE);
            c.setTextDatum(lgfx::middle_center);
            c.setTextColor(hasPrev ? Theme::FG : Theme::DIM, (_navHighlight == 0) ? Theme::PALE : Theme::BG);
            c.drawString("<< prev", SCR_W / 4, y + itemH / 2 + 1);
            c.drawFastVLine(SCR_W / 2, y, itemH, Theme::DIM);
            c.setTextColor(hasNext ? Theme::FG : Theme::DIM, (_navHighlight == 1) ? Theme::PALE : Theme::BG);
            c.drawString("next >>", SCR_W * 3 / 4, y + itemH / 2 + 1);
            c.drawFastHLine(0, y + itemH - 1, SCR_W, Theme::DIM);
            continue;
        }

        int itemIdx = scroll + (slot - firstItemSlot);
        if (itemIdx < 0 || itemIdx >= nItems) continue;

        if (_menuState == MenuState::Root) {
            const bool locked = _currentService != nullptr;
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
