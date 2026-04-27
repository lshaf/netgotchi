#include "Virus.h"
#include "Theme.h"

void Virus::draw(M5Canvas& c, uint32_t ms, bool sleeping) {
    c.fillRect(X0, Y0, SIDE, SIDE, Theme::BG);

    const int cx = X0 + SIDE / 2;
    const int cy = Y0 + SIDE / 2;

    if (sleeping) {
        // ── Sleeping ──────────────────────────────────────────
        for (int i = 1; i <= 2; i++) {
            c.drawPixel(cx,         cy - R - i, Theme::DIM);
            c.drawPixel(cx,         cy + R + i, Theme::DIM);
            c.drawPixel(cx - R - i, cy,         Theme::DIM);
            c.drawPixel(cx + R + i, cy,         Theme::DIM);
        }
        c.drawPixel(cx - 8, cy - 8, Theme::DIM);
        c.drawPixel(cx + 8, cy - 8, Theme::DIM);
        c.drawPixel(cx - 8, cy + 8, Theme::DIM);
        c.drawPixel(cx + 8, cy + 8, Theme::DIM);

        c.fillCircle(cx, cy, R, Theme::BG);
        c.drawCircle(cx, cy, R, Theme::DIM);

        // Relaxed brows
        c.drawFastHLine(cx - 5, cy - 5, 3, Theme::FG);
        c.drawFastHLine(cx + 2, cy - 5, 3, Theme::FG);

        // Closed eyes (always)
        c.drawFastHLine(cx - 4, cy - 3, 3, Theme::FG);
        c.drawFastHLine(cx + 1, cy - 3, 2, Theme::FG);

        // Flat mouth
        c.drawFastHLine(cx - 2, cy + 3, 5, Theme::DIM);

        // Zzz — two stacked pixel-art z's, offset diagonally
        const int zx = cx + 7, zy = cy - R - 6;
        c.drawFastHLine(zx,     zy,     3, Theme::FG);
        c.drawPixel    (zx + 1, zy + 1, Theme::FG);
        c.drawFastHLine(zx,     zy + 2, 3, Theme::FG);
        c.drawFastHLine(zx + 3, zy + 3, 2, Theme::PALE);
        c.drawPixel    (zx + 4, zy + 4, Theme::PALE);
        c.drawFastHLine(zx + 3, zy + 5, 2, Theme::PALE);
        return;
    }

    // ── Alive ─────────────────────────────────────────────────
    const bool tick  = (ms / 400) & 1;    // spike pulse 400 ms each state
    const bool blink = (ms % 3000) < 100; // blink 100 ms every 3 s

    const int sLen = tick ? 4 : 2;        // cardinal spike shaft length
    const int dLen = tick ? 3 : 1;        // diagonal spike shaft length

    // ── Cardinal spikes + round bulb tips ────────────────────────
    for (int i = 1; i <= sLen; i++) {
        c.drawPixel(cx,         cy - R - i, Theme::FG);
        c.drawPixel(cx,         cy + R + i, Theme::FG);
        c.drawPixel(cx - R - i, cy,         Theme::FG);
        c.drawPixel(cx + R + i, cy,         Theme::FG);
    }
    c.fillRect(cx - 1,            cy - R - sLen - 1, 3, 2, Theme::FG); // top bulb
    c.fillRect(cx - 1,            cy + R + sLen,     3, 2, Theme::FG); // bottom bulb
    c.fillRect(cx - R - sLen - 1, cy - 1,            2, 3, Theme::FG); // left bulb
    c.fillRect(cx + R + sLen - 1, cy - 1,            2, 3, Theme::FG); // right bulb

    // ── Diagonal spikes + dot tips ────────────────────────────────
    for (int i = 0; i < dLen; i++) {
        c.drawPixel(cx - 8 - i, cy - 8 - i, Theme::FG);
        c.drawPixel(cx + 8 + i, cy - 8 - i, Theme::FG);
        c.drawPixel(cx - 8 - i, cy + 8 + i, Theme::FG);
        c.drawPixel(cx + 8 + i, cy + 8 + i, Theme::FG);
    }
    const int dt = 8 + dLen;
    c.fillRect(cx - dt - 1, cy - dt - 1, 2, 2, Theme::FG);
    c.fillRect(cx + dt - 1, cy - dt - 1, 2, 2, Theme::FG);
    c.fillRect(cx - dt - 1, cy + dt - 1, 2, 2, Theme::FG);
    c.fillRect(cx + dt - 1, cy + dt - 1, 2, 2, Theme::FG);

    // ── Body ──────────────────────────────────────────────────────
    c.fillCircle(cx, cy, R, Theme::BG);
    c.drawCircle(cx, cy, R, Theme::FG);

    // ── Angry brows ───────────────────────────────────────────────
    c.drawPixel(cx - 5, cy - 5, Theme::FG);
    c.drawPixel(cx - 4, cy - 5, Theme::FG);
    c.drawPixel(cx - 3, cy - 4, Theme::FG);
    c.drawPixel(cx + 2, cy - 4, Theme::FG);
    c.drawPixel(cx + 3, cy - 5, Theme::FG);
    c.drawPixel(cx + 4, cy - 5, Theme::FG);

    // ── Eyes: open = 3×2 left + 2×2 right, closed = flat lines ──
    if (!blink) {
        c.fillRect(cx - 4, cy - 3, 3, 2, Theme::FG);
        c.fillRect(cx + 1, cy - 3, 2, 2, Theme::FG);
    } else {
        c.drawFastHLine(cx - 4, cy - 3, 3, Theme::FG);
        c.drawFastHLine(cx + 1, cy - 3, 2, Theme::FG);
    }

    // ── Mouth: top lip + teeth gaps ───────────────────────────────
    c.drawFastHLine(cx - 3, cy + 3, 7, Theme::FG);
    for (int x = cx - 2; x <= cx + 2; x += 2)
        c.drawPixel(x, cy + 4, Theme::FG);
}
