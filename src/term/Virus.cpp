#include "Virus.h"
#include "Theme.h"

void Virus::draw(M5Canvas& c, uint32_t ms, State state) {
    c.fillRect(X0, Y0, SIDE, SIDE, Theme::BG);

    const int cx = X0 + SIDE / 2;
    const int cy = Y0 + SIDE / 2;

    // ── Sleep ─────────────────────────────────────────────────────
    if (state == State::Sleep) {
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
        // Closed eyes
        c.drawFastHLine(cx - 4, cy - 3, 3, Theme::FG);
        c.drawFastHLine(cx + 1, cy - 3, 2, Theme::FG);
        // Flat mouth
        c.drawFastHLine(cx - 2, cy + 3, 5, Theme::DIM);

        // Zzz
        const int zx = cx + 7, zy = cy - R - 6;
        c.drawFastHLine(zx,     zy,     3, Theme::FG);
        c.drawPixel    (zx + 1, zy + 1, Theme::FG);
        c.drawFastHLine(zx,     zy + 2, 3, Theme::FG);
        c.drawFastHLine(zx + 3, zy + 3, 2, Theme::PALE);
        c.drawPixel    (zx + 4, zy + 4, Theme::PALE);
        c.drawFastHLine(zx + 3, zy + 5, 2, Theme::PALE);
        return;
    }

    // ── Idle ──────────────────────────────────────────────────────
    if (state == State::Idle) {
        c.fillCircle(cx, cy, R, Theme::BG);
        c.drawCircle(cx, cy, R, Theme::DIM);

        // Flat neutral brows
        c.drawFastHLine(cx - 5, cy - 5, 4, Theme::DIM);
        c.drawFastHLine(cx + 1, cy - 5, 4, Theme::DIM);

        // Half-lidded bored eyes — slow blink every 4 s
        bool blink = (ms % 4000) < 100;
        if (!blink) {
            c.fillRect(cx - 4, cy - 3, 3, 1, Theme::DIM);
            c.fillRect(cx + 1, cy - 3, 2, 1, Theme::DIM);
        }

        // Flat bored mouth with slight left droop
        c.drawFastHLine(cx - 2, cy + 3, 5, Theme::DIM);
        c.drawPixel(cx - 3, cy + 4, Theme::DIM);
        return;
    }

    // ── Active ────────────────────────────────────────────────────
    if (state == State::Active) {
        const bool tick  = (ms / 400) & 1;
        const bool blink = (ms % 3000) < 100;
        const int sLen = tick ? 4 : 2;
        const int dLen = tick ? 3 : 1;

        for (int i = 1; i <= sLen; i++) {
            c.drawPixel(cx,         cy - R - i, Theme::FG);
            c.drawPixel(cx,         cy + R + i, Theme::FG);
            c.drawPixel(cx - R - i, cy,         Theme::FG);
            c.drawPixel(cx + R + i, cy,         Theme::FG);
        }
        c.fillRect(cx - 1,            cy - R - sLen - 1, 3, 2, Theme::FG);
        c.fillRect(cx - 1,            cy + R + sLen,     3, 2, Theme::FG);
        c.fillRect(cx - R - sLen - 1, cy - 1,            2, 3, Theme::FG);
        c.fillRect(cx + R + sLen - 1, cy - 1,            2, 3, Theme::FG);

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

        c.fillCircle(cx, cy, R, Theme::BG);
        c.drawCircle(cx, cy, R, Theme::FG);

        // Angry brows
        c.drawPixel(cx - 5, cy - 5, Theme::FG);
        c.drawPixel(cx - 4, cy - 5, Theme::FG);
        c.drawPixel(cx - 3, cy - 4, Theme::FG);
        c.drawPixel(cx + 2, cy - 4, Theme::FG);
        c.drawPixel(cx + 3, cy - 5, Theme::FG);
        c.drawPixel(cx + 4, cy - 5, Theme::FG);

        if (!blink) {
            c.fillRect(cx - 4, cy - 3, 3, 2, Theme::FG);
            c.fillRect(cx + 1, cy - 3, 2, 2, Theme::FG);
        } else {
            c.drawFastHLine(cx - 4, cy - 3, 3, Theme::FG);
            c.drawFastHLine(cx + 1, cy - 3, 2, Theme::FG);
        }

        // Chomping mouth
        c.drawFastHLine(cx - 3, cy + 3, 7, Theme::FG);
        for (int x = cx - 2; x <= cx + 2; x += 2)
            c.drawPixel(x, cy + 4, Theme::FG);
        return;
    }

    // ── Guard ─────────────────────────────────────────────────────
    if (state == State::Guard) {
        // Steady short pale spikes — present but not aggressive
        for (int i = 1; i <= 2; i++) {
            c.drawPixel(cx,         cy - R - i, Theme::PALE);
            c.drawPixel(cx,         cy + R + i, Theme::PALE);
            c.drawPixel(cx - R - i, cy,         Theme::PALE);
            c.drawPixel(cx + R + i, cy,         Theme::PALE);
        }
        c.fillRect(cx - 1, cy - R - 3, 3, 1, Theme::PALE);
        c.fillRect(cx - 1, cy + R + 2, 3, 1, Theme::PALE);
        c.fillRect(cx - R - 3, cy - 1, 1, 3, Theme::PALE);
        c.fillRect(cx + R + 2, cy - 1, 1, 3, Theme::PALE);

        c.fillCircle(cx, cy, R, Theme::BG);
        c.drawCircle(cx, cy, R, Theme::FG);

        // Raised alert brows (higher than normal)
        c.drawFastHLine(cx - 5, cy - 6, 4, Theme::FG);
        c.drawFastHLine(cx + 1, cy - 6, 4, Theme::FG);

        // Scanning eyes — dart left / center / right every 1.2 s
        int eyePhase = (int)(ms / 1200) % 3;
        int eyeOff   = (eyePhase == 0) ? -1 : (eyePhase == 2) ? 1 : 0;
        c.fillRect(cx - 4 + eyeOff, cy - 3, 3, 2, Theme::FG);
        c.fillRect(cx + 1 + eyeOff, cy - 3, 2, 2, Theme::FG);

        // Straight watchful mouth
        c.drawFastHLine(cx - 2, cy + 3, 5, Theme::FG);
        return;
    }

    // ── Decrypting ────────────────────────────────────────────────
    {
        // Rotating comet: 8 cardinal+diagonal orbit positions
        // order is clockwise starting from right
        static const int8_t sx[8] = { 13,  9,  0, -9, -13, -9,  0,  9 };
        static const int8_t sy[8] = {  0,  9, 13,  9,   0, -9, -13, -9 };
        const int step = (int)(ms / 100) % 8;
        c.drawPixel(cx + sx[(step + 6) % 8], cy + sy[(step + 6) % 8], Theme::DIM);
        c.drawPixel(cx + sx[(step + 7) % 8], cy + sy[(step + 7) % 8], Theme::PALE);
        c.drawPixel(cx + sx[step],           cy + sy[step],           Theme::FG);

        c.fillCircle(cx, cy, R, Theme::BG);
        c.drawCircle(cx, cy, R, Theme::FG);

        // Concentrated brows (angled inward, close together)
        c.drawPixel(cx - 4, cy - 5, Theme::FG);
        c.drawPixel(cx - 3, cy - 4, Theme::FG);
        c.drawPixel(cx + 2, cy - 4, Theme::FG);
        c.drawPixel(cx + 3, cy - 5, Theme::FG);

        // Squinting eyes (1 px tall, no blink — too focused)
        c.drawFastHLine(cx - 4, cy - 3, 3, Theme::FG);
        c.drawFastHLine(cx + 1, cy - 3, 2, Theme::FG);

        // Tight determined mouth
        c.drawFastHLine(cx - 2, cy + 3, 5, Theme::FG);
    }
}
