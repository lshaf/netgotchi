#include "Virus.h"
#include "Theme.h"

// Robot head geometry (all relative to cx, cy — the canvas center)
static constexpr int HW = 9;   // head half-width: edges at cx±9
static constexpr int HH = 7;   // head half-height: edges at cy±7

static void _head(M5Canvas& c, int cx, int cy, uint16_t col) {
    c.drawRect(cx - HW, cy - HH, 2 * HW + 1, 2 * HH + 1, col);
}

void Virus::draw(M5Canvas& c, uint32_t ms, State state) {
    c.fillRect(X0, Y0, SIDE, SIDE, Theme::BG);
    const int cx = X0 + SIDE / 2;
    const int cy = Y0 + SIDE / 2;

    // ── Sleep ─────────────────────────────────────────────────────
    if (state == State::Sleep) {
        // Drooping antenna (ball droops right)
        c.drawFastVLine(cx,     cy - HH - 3, 3,  Theme::DIM);   // stalk
        c.drawPixel    (cx + 1, cy - HH - 4,     Theme::DIM);   // ball droops right

        // Tiny dim ear stubs
        c.drawPixel(cx - HW - 1, cy, Theme::DIM);
        c.drawPixel(cx + HW + 1, cy, Theme::DIM);

        // Head (dim border)
        _head(c, cx, cy, Theme::DIM);

        // Relaxed flat brows
        c.drawFastHLine(cx - 6, cy - 4, 3, Theme::DIM);
        c.drawFastHLine(cx + 3, cy - 4, 3, Theme::DIM);

        // Closed eyes
        c.drawFastHLine(cx - 6, cy - 2, 3, Theme::FG);
        c.drawFastHLine(cx + 3, cy - 2, 3, Theme::FG);

        // Speaker grille mouth (dim)
        c.drawPixel(cx - 4, cy + 5, Theme::DIM);
        c.drawPixel(cx - 2, cy + 5, Theme::DIM);
        c.drawPixel(cx,     cy + 5, Theme::DIM);
        c.drawPixel(cx + 2, cy + 5, Theme::DIM);
        c.drawPixel(cx + 4, cy + 5, Theme::DIM);

        // ZZZ (upper-right of cell, above antenna)
        const int zx = cx + 4, zy = cy - HH - 7;   // zy = cy-14 = 6 ≥ Y0
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
        c.drawFastVLine(cx, cy - HH - 3, 3,  Theme::DIM);   // stalk
        c.drawPixel    (cx, cy - HH - 4,     Theme::DIM);   // ball

        // Dim ear stubs
        c.drawPixel(cx - HW - 1, cy, Theme::DIM);
        c.drawPixel(cx + HW + 1, cy, Theme::DIM);

        // Head (dim border)
        _head(c, cx, cy, Theme::DIM);

        // Flat neutral brows
        c.drawFastHLine(cx - 6, cy - 4, 3, Theme::DIM);
        c.drawFastHLine(cx + 3, cy - 4, 3, Theme::DIM);

        // Half-lidded eyes — slow blink every 4 s
        bool blink = (ms % 4000) < 100;
        if (!blink) {
            c.fillRect(cx - 6, cy - 2, 3, 1, Theme::DIM);
            c.fillRect(cx + 3, cy - 2, 3, 1, Theme::DIM);
        }

        // Speaker grille mouth (dim)
        c.drawPixel(cx - 4, cy + 5, Theme::DIM);
        c.drawPixel(cx - 2, cy + 5, Theme::DIM);
        c.drawPixel(cx,     cy + 5, Theme::DIM);
        c.drawPixel(cx + 2, cy + 5, Theme::DIM);
        c.drawPixel(cx + 4, cy + 5, Theme::DIM);
        return;
    }

    // ── Active ────────────────────────────────────────────────────
    if (state == State::Active) {
        const bool tick  = (ms / 400) & 1;
        const bool blink = (ms % 3000) < 100;

        // Pulsing antenna horn (longer on tick)
        int antH = tick ? 5 : 3;
        c.drawFastVLine(cx, cy - HH - 1, antH, Theme::FG);           // stalk up
        c.fillRect(cx - 1, cy - HH - antH - 1, 3, 1, Theme::FG);    // tip cap

        // Pulsing ear side-spikes
        int earW = tick ? 2 : 1;
        for (int i = 1; i <= earW; i++) {
            c.drawFastVLine(cx - HW - i, cy - 1, 3, Theme::FG);
            c.drawFastVLine(cx + HW + i, cy - 1, 3, Theme::FG);
        }
        // Ear caps
        c.fillRect(cx - HW - earW - 1, cy - 1, 1, 3, Theme::FG);
        c.fillRect(cx + HW + earW,     cy - 1, 1, 3, Theme::FG);

        // Head (FG border)
        _head(c, cx, cy, Theme::FG);

        // Angry V-brows
        c.drawPixel(cx - 6, cy - 5, Theme::FG);
        c.drawPixel(cx - 5, cy - 4, Theme::FG);
        c.drawPixel(cx - 4, cy - 4, Theme::FG);
        c.drawPixel(cx + 3, cy - 4, Theme::FG);
        c.drawPixel(cx + 4, cy - 4, Theme::FG);
        c.drawPixel(cx + 5, cy - 5, Theme::FG);

        // Eyes: solid rects or blink to lines
        if (!blink) {
            c.fillRect(cx - 6, cy - 2, 3, 2, Theme::FG);
            c.fillRect(cx + 3, cy - 2, 3, 2, Theme::FG);
        } else {
            c.drawFastHLine(cx - 6, cy - 2, 3, Theme::FG);
            c.drawFastHLine(cx + 3, cy - 2, 3, Theme::FG);
        }

        // Chomping mouth: open on tick, closed off tick
        if (tick) {
            c.drawFastHLine(cx - 4, cy + 4, 9, Theme::FG);
            c.drawFastHLine(cx - 4, cy + 6, 9, Theme::FG);
            for (int x = cx - 3; x <= cx + 4; x += 2)
                c.drawPixel(x, cy + 5, Theme::FG);
        } else {
            c.drawFastHLine(cx - 4, cy + 5, 9, Theme::FG);
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

        // Raised flat brows
        c.drawFastHLine(cx - 6, cy - 4, 3, Theme::FG);
        c.drawFastHLine(cx + 3, cy - 4, 3, Theme::FG);

        // Scanning eyes: shift ±1 every 1.2 s
        int eyePhase = (int)(ms / 1200) % 3;
        int eyeOff   = (eyePhase == 0) ? -1 : (eyePhase == 2) ? 1 : 0;
        c.fillRect(cx - 6 + eyeOff, cy - 2, 3, 2, Theme::FG);
        c.fillRect(cx + 3 + eyeOff, cy - 2, 3, 2, Theme::FG);

        // Watchful grille mouth
        c.drawPixel(cx - 4, cy + 5, Theme::FG);
        c.drawPixel(cx - 2, cy + 5, Theme::FG);
        c.drawPixel(cx,     cy + 5, Theme::FG);
        c.drawPixel(cx + 2, cy + 5, Theme::FG);
        c.drawPixel(cx + 4, cy + 5, Theme::FG);
        return;
    }

    // ── Decrypting ────────────────────────────────────────────────
    {
        // Orbit comet outside the 19×15 head (HW=9, HH=7).
        // All 8 positions satisfy |sx|>9 or |sy|>7 — none intersect the head rect.
        static const int8_t sx[8] = { 13, 11,  0, -11, -13, -11,  0,  11 };
        static const int8_t sy[8] = {  0,  9, 13,   9,   0,  -9, -13,  -9 };
        const int step = (int)(ms / 100) % 8;
        c.drawPixel(cx + sx[(step + 6) % 8], cy + sy[(step + 6) % 8], Theme::DIM);
        c.drawPixel(cx + sx[(step + 7) % 8], cy + sy[(step + 7) % 8], Theme::PALE);
        c.drawPixel(cx + sx[step],           cy + sy[step],           Theme::FG);

        // Dim antenna stalk only (focused — no ball)
        c.drawFastVLine(cx, cy - HH - 3, 3, Theme::DIM);

        // Head (FG border)
        _head(c, cx, cy, Theme::FG);

        // Concentrated inward V-brows
        c.drawPixel(cx - 5, cy - 5, Theme::FG);
        c.drawPixel(cx - 4, cy - 4, Theme::FG);
        c.drawPixel(cx + 3, cy - 4, Theme::FG);
        c.drawPixel(cx + 4, cy - 5, Theme::FG);

        // Processing eyes: single pixel scans across each eye every 150 ms
        int scanPos = (int)(ms / 150) % 3;
        // Eye sockets (dim background)
        c.fillRect(cx - 6, cy - 2, 3, 2, Theme::DIM);
        c.fillRect(cx + 3, cy - 2, 3, 2, Theme::DIM);
        // Scanning pixel (FG)
        c.drawPixel(cx - 6 + scanPos, cy - 1, Theme::FG);
        c.drawPixel(cx + 3 + scanPos, cy - 1, Theme::FG);

        // Tight focused mouth
        c.drawFastHLine(cx - 4, cy + 5, 9, Theme::FG);
    }
}
