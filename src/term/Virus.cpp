#include "Virus.h"
#include "Theme.h"

// Robot head geometry (all relative to cx, cy — the canvas center)
static constexpr int HW = 11;  // head half-width:  edges at cx±11 (23px wide)
static constexpr int HH = 9;   // head half-height: edges at cy±9  (19px tall)

// 2px thick border — draw outer rect then inner rect one pixel inside
static void _head(TFT_eSprite& c, int cx, int cy, uint16_t col) {
    c.drawRect(cx - HW,     cy - HH,     2*HW + 1,   2*HH + 1,   col);
    c.drawRect(cx - HW + 1, cy - HH + 1, 2*HW - 1,   2*HH - 1,   col);
}

void Virus::draw(TFT_eSprite& c, uint32_t ms, State state) {
    c.fillRect(X0, Y0, SIDE, SIDE, Theme::BG);
    const int cx = X0 + SIDE / 2;
    const int cy = Y0 + SIDE / 2;

    // ── Sleep ─────────────────────────────────────────────────────
    if (state == State::Sleep) {
        // Drooping antenna (short stalk, ball droops right)
        c.drawFastVLine(cx,     cy - HH - 2, 2,  Theme::DIM);
        c.drawPixel    (cx + 1, cy - HH - 1,     Theme::DIM);

        // Tiny ear stubs
        c.drawPixel(cx - HW - 1, cy, Theme::DIM);
        c.drawPixel(cx + HW + 1, cy, Theme::DIM);

        // Head (dim border)
        _head(c, cx, cy, Theme::DIM);

        // Relaxed flat brows (2px tall)
        c.fillRect(cx - 8, cy - 5, 4, 2, Theme::DIM);
        c.fillRect(cx + 4, cy - 5, 4, 2, Theme::DIM);

        // Closed eyes (hlines)
        c.drawFastHLine(cx - 8, cy - 2, 4, Theme::FG);
        c.drawFastHLine(cx + 4, cy - 2, 4, Theme::FG);

        // Speaker grille mouth (dim)
        c.drawPixel(cx - 6, cy + 7, Theme::DIM);
        c.drawPixel(cx - 3, cy + 7, Theme::DIM);
        c.drawPixel(cx,     cy + 7, Theme::DIM);
        c.drawPixel(cx + 3, cy + 7, Theme::DIM);
        c.drawPixel(cx + 6, cy + 7, Theme::DIM);

        // ZZZ (upper-right of cell, above antenna); zy = cy-HH-7 = 4 = Y0
        const int zx = cx + 4, zy = cy - HH - 7;
        c.drawFastHLine(zx,     zy,     3, Theme::FG);
        c.drawPixel    (zx + 1, zy + 1, Theme::FG);
        c.drawFastHLine(zx,     zy + 2, 3, Theme::FG);
        c.drawFastHLine(zx + 2, zy + 3, 2, Theme::PALE);
        c.drawPixel    (zx + 3, zy + 4, Theme::PALE);
        c.drawFastHLine(zx + 2, zy + 5, 2, Theme::PALE);
        return;
    }

    // ── Idle ──────────────────────────────────────────────────────
    if (state == State::Idle) {
        // Steady dim antenna
        c.drawFastVLine(cx, cy - HH - 3, 3,  Theme::DIM);
        c.drawPixel    (cx, cy - HH - 4,     Theme::DIM);

        // Dim ear stubs
        c.drawPixel(cx - HW - 1, cy, Theme::DIM);
        c.drawPixel(cx + HW + 1, cy, Theme::DIM);

        // Head (dim border)
        _head(c, cx, cy, Theme::DIM);

        // Flat neutral brows (2px tall)
        c.fillRect(cx - 8, cy - 5, 4, 2, Theme::DIM);
        c.fillRect(cx + 4, cy - 5, 4, 2, Theme::DIM);

        // Half-lidded eyes — slow blink every 4s
        bool blink = (ms % 4000) < 100;
        if (!blink) {
            c.fillRect(cx - 8, cy - 3, 4, 2, Theme::DIM);
            c.fillRect(cx + 4, cy - 3, 4, 2, Theme::DIM);
        }

        // Speaker grille mouth (dim)
        c.drawPixel(cx - 6, cy + 7, Theme::DIM);
        c.drawPixel(cx - 3, cy + 7, Theme::DIM);
        c.drawPixel(cx,     cy + 7, Theme::DIM);
        c.drawPixel(cx + 3, cy + 7, Theme::DIM);
        c.drawPixel(cx + 6, cy + 7, Theme::DIM);
        return;
    }

    // ── Active ────────────────────────────────────────────────────
    if (state == State::Active) {
        const bool tick  = (ms / 400) & 1;
        const bool blink = (ms % 3000) < 100;

        // Pulsing antenna — 3px wide block above head
        int antH = tick ? 5 : 3;
        c.fillRect(cx - 1, cy - HH - antH, 3, antH, Theme::FG);

        // Pulsing ear side-spikes + outer cap (symmetric)
        int earW = tick ? 2 : 1;
        for (int i = 1; i <= earW; i++) {
            c.drawFastVLine(cx - HW - i, cy - 1, 3, Theme::FG);
            c.drawFastVLine(cx + HW + i, cy - 1, 3, Theme::FG);
        }
        c.fillRect(cx - HW - earW - 1, cy - 1, 1, 3, Theme::FG);
        c.fillRect(cx + HW + earW + 1, cy - 1, 1, 3, Theme::FG);

        // Head (FG border)
        _head(c, cx, cy, Theme::FG);

        // Angry V-brows (outer corner high)
        c.drawPixel(cx - 8, cy - 6, Theme::FG);
        c.drawPixel(cx - 7, cy - 5, Theme::FG);
        c.drawPixel(cx - 6, cy - 5, Theme::FG);
        c.drawPixel(cx + 5, cy - 5, Theme::FG);
        c.drawPixel(cx + 6, cy - 5, Theme::FG);
        c.drawPixel(cx + 7, cy - 6, Theme::FG);

        // Eyes: solid 4×2 or blink to lines
        if (!blink) {
            c.fillRect(cx - 8, cy - 3, 4, 2, Theme::FG);
            c.fillRect(cx + 4, cy - 3, 4, 2, Theme::FG);
        } else {
            c.drawFastHLine(cx - 8, cy - 3, 4, Theme::FG);
            c.drawFastHLine(cx + 4, cy - 3, 4, Theme::FG);
        }

        // Chomping mouth: open on tick, 2px bar off tick
        if (tick) {
            c.drawFastHLine(cx - 6, cy + 4, 13, Theme::FG);  // top lip
            c.drawFastHLine(cx - 6, cy + 7, 13, Theme::FG);  // bottom lip
            for (int x = cx - 5; x <= cx + 5; x += 2)
                c.drawPixel(x, cy + 5, Theme::FG);            // teeth
        } else {
            c.fillRect(cx - 6, cy + 5, 13, 2, Theme::FG);
        }
        return;
    }

    // ── Guard ─────────────────────────────────────────────────────
    if (state == State::Guard) {
        // Steady pale antenna
        c.drawFastVLine(cx, cy - HH - 3, 3,  Theme::PALE);
        c.drawPixel    (cx, cy - HH - 4,     Theme::PALE);

        // Pale ear stubs (3px tall each side)
        c.drawFastVLine(cx - HW - 1, cy - 1, 3, Theme::PALE);
        c.drawFastVLine(cx + HW + 1, cy - 1, 3, Theme::PALE);

        // Head (FG border)
        _head(c, cx, cy, Theme::FG);

        // Raised flat brows (2px tall)
        c.fillRect(cx - 8, cy - 5, 4, 2, Theme::FG);
        c.fillRect(cx + 4, cy - 5, 4, 2, Theme::FG);

        // Scanning eyes: shift ±1 every 1.2s
        int eyePhase = (int)(ms / 1200) % 3;
        int eyeOff   = (eyePhase == 0) ? -1 : (eyePhase == 2) ? 1 : 0;
        c.fillRect(cx - 8 + eyeOff, cy - 3, 4, 2, Theme::FG);
        c.fillRect(cx + 4 + eyeOff, cy - 3, 4, 2, Theme::FG);

        // Watchful grille mouth
        c.drawPixel(cx - 6, cy + 7, Theme::FG);
        c.drawPixel(cx - 3, cy + 7, Theme::FG);
        c.drawPixel(cx,     cy + 7, Theme::FG);
        c.drawPixel(cx + 3, cy + 7, Theme::FG);
        c.drawPixel(cx + 6, cy + 7, Theme::FG);
        return;
    }

    // ── Trap ──────────────────────────────────────────────────────
    if (state == State::Trap) {
        // Pulsing beacon antenna: FG → PALE → DIM breath cycle every 1.2s
        uint32_t beat = ms % 1200;
        uint16_t ballCol = (beat < 300) ? Theme::FG : (beat < 700) ? Theme::PALE : Theme::DIM;
        c.drawFastVLine(cx, cy - HH - 3, 3, Theme::DIM);  // stalk
        c.drawPixel    (cx, cy - HH - 4,    ballCol);     // pulsing ball

        // PALE ear stubs (passive, quiet)
        c.drawPixel(cx - HW - 1, cy, Theme::PALE);
        c.drawPixel(cx + HW + 1, cy, Theme::PALE);

        // Head (FG border — actively listening)
        _head(c, cx, cy, Theme::FG);

        // Curious inverted-V brows: inner corner high (inverse of angry Active brows)
        c.drawPixel(cx - 8, cy - 5, Theme::PALE);
        c.drawPixel(cx - 7, cy - 5, Theme::PALE);
        c.drawPixel(cx - 6, cy - 6, Theme::PALE);  // inner left high
        c.drawPixel(cx + 5, cy - 6, Theme::PALE);  // inner right high
        c.drawPixel(cx + 6, cy - 5, Theme::PALE);
        c.drawPixel(cx + 7, cy - 5, Theme::PALE);

        // Small 2×2 eyes (rounder/more curious than 4×2) with slow blink every 3s
        bool tblink = (ms % 3000) < 120;
        if (!tblink) {
            c.fillRect(cx - 7, cy - 3, 2, 2, Theme::FG);
            c.fillRect(cx + 5, cy - 3, 2, 2, Theme::FG);
        }

        // Upturned mouth: flat center + uptick pixel on each end (curious smile)
        c.drawFastHLine(cx - 4, cy + 6, 9, Theme::PALE);
        c.drawPixel(cx - 5, cy + 5, Theme::PALE);
        c.drawPixel(cx + 5, cy + 5, Theme::PALE);
        return;
    }

    // ── Decrypting ────────────────────────────────────────────────
    {
        // Orbit comet outside the 23×19 head (HW=11, HH=9).
        // All 8 positions satisfy |sx|>11 or |sy|>9 — none intersect the head rect.
        static const int8_t sx[8] = { 14,  12,  0, -12, -14, -12,  0,  12 };
        static const int8_t sy[8] = {  0,  10, 14,  10,   0, -10, -14, -10 };
        const int step = (int)(ms / 100) % 8;
        c.drawPixel(cx + sx[(step + 6) % 8], cy + sy[(step + 6) % 8], Theme::DIM);
        c.drawPixel(cx + sx[(step + 7) % 8], cy + sy[(step + 7) % 8], Theme::PALE);
        c.drawPixel(cx + sx[step],           cy + sy[step],           Theme::FG);

        // Dim antenna stalk only (focused — no ball)
        c.drawFastVLine(cx, cy - HH - 3, 3, Theme::DIM);

        // Head (FG border)
        _head(c, cx, cy, Theme::FG);

        // Concentrated inward V-brows (inner corner high)
        c.drawPixel(cx - 6, cy - 6, Theme::FG);
        c.drawPixel(cx - 5, cy - 5, Theme::FG);
        c.drawPixel(cx + 4, cy - 5, Theme::FG);
        c.drawPixel(cx + 5, cy - 6, Theme::FG);

        // Processing eyes: 4-wide socket, single pixel scans across every 150ms
        int scanPos = (int)(ms / 150) % 4;
        c.fillRect(cx - 8, cy - 3, 4, 2, Theme::DIM);
        c.fillRect(cx + 4, cy - 3, 4, 2, Theme::DIM);
        c.drawPixel(cx - 8 + scanPos, cy - 2, Theme::FG);
        c.drawPixel(cx + 4 + scanPos, cy - 2, Theme::FG);

        // Tight focused mouth (2px tall)
        c.fillRect(cx - 6, cy + 6, 13, 2, Theme::FG);
    }
}
