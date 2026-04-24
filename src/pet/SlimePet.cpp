#include "SlimePet.h"
#include "../core/Palette.h"
#include <M5Unified.h>
#include <cmath>
#include <cstring>

static constexpr uint16_t FRAME_MS[] = { 100, 115, 250, 310 };

// ── Construction ──────────────────────────────────────────────

SlimePet::SlimePet() : _x(SCREEN_W / 2.0f) {}

// ── Public interface ──────────────────────────────────────────

void SlimePet::setAnim(Anim a) {
    if (_anim == a) return;
    _anim         = a;
    _frame        = 0;
    _lastMs       = 0;
    _wallPauseEnd = 0;
}

void SlimePet::setSpeech(const char* msg) {
    _speech      = msg;
    _speechStart = 0;
}

void SlimePet::update(uint32_t ms) {
    _nowMs = ms;
    if (_speech && _speechStart == 0) _speechStart = ms;
    if (_speech && _speechStart > 0 && (ms - _speechStart) >= SPEECH_DUR)
        _speech = nullptr;

    uint32_t iv = FRAME_MS[static_cast<uint8_t>(_anim)];
    if (ms - _lastMs < iv) return;
    _lastMs = ms;
    _frame++;

    if (_anim == Anim::Walk) {
        const int su     = SCREEN_H / 10;
        const int bW     = su + su / 4;
        const int rBound = SCREEN_W - bW - 2;
        const int lBound = bW + 2;

        if (_pinCenter) {
            // Attack mode: snap to center, use Walk frames for jump arc
            float dx = SCREEN_W / 2.0f - _x;
            if (fabsf(dx) > 2.0f) _x += dx * 0.2f;
        } else if (_nowMs < _wallPauseEnd) {
            // Paused at corner — hold position, frames still advance
        } else {
            float speed = _stayRight ? 15.0f : 2.8f;
            _x += _walkDir * speed;
            if (_x > rBound) {
                _x = (float)rBound;
                if (!_stayRight) { _walkDir = -1; _wallPauseEnd = _nowMs + 1000; }
            }
            if (_x < lBound) { _x = (float)lBound; _walkDir = 1; _wallPauseEnd = _nowMs + 1000; }
        }
    } else {
        if (!_stayRight) {
            float dx = SCREEN_W / 2.0f - _x;
            if (fabsf(dx) > 6.0f) _x += dx * 0.12f;
        }
    }
}

void SlimePet::draw(M5Canvas& c) const {
    Visual v = _currentVisual();
    _renderBody(c, v);
    _renderEyes(c, v);
    if (v.thinkR > 0) _renderThinkBubble(c, v);
    _renderSpeechBubble(c, v);
}

// ── Frame generators ──────────────────────────────────────────

SlimePet::Visual SlimePet::_currentVisual() const {
    switch (_anim) {
        case Anim::Walk:  return _walkVisual();
        case Anim::Talk:  return _talkVisual();
        case Anim::Think: return _thinkVisual();
        default:          return _idleVisual();
    }
}

SlimePet::Visual SlimePet::_idleVisual() const {
    const int su = SCREEN_H / 10, s_b = su + su / 4;
    static const int8_t YO[] = { 0, 0,-4,-4, 0, 0, 4, 4 };
    int fi = _frame % 8;
    uint8_t eyeState = (fi == 5) ? 1 : (fi == 6) ? 2 : 0;
    return { (int)_x, GROUND_Y - s_b + YO[fi], s_b, s_b, 1, 1, eyeState, 0, 0 };
}

SlimePet::Visual SlimePet::_walkVisual() const {
    const int su = SCREEN_H / 10, s_b = su + su / 4;
    static const int8_t YF[] = { 0,-1,-3,-1, 0,-1,-3,-1 };
    int fi = _frame % 8;
    int yo = YF[fi] * su / 3;
    return { (int)_x, GROUND_Y - s_b + yo, s_b, s_b, _walkDir, 1, 0, 0, 0 };
}

SlimePet::Visual SlimePet::_talkVisual() const {
    const int su = SCREEN_H / 10, s_b = su + su / 4;
    static const uint8_t MOUTH[] = { 0, 1, 2, 2, 1, 0 };
    static const int8_t  YO[]    = { 0, 0,-4,-4, 0, 0 };
    int fi = _frame % 6;
    return { (int)_x, GROUND_Y - s_b + YO[fi], s_b, s_b, 1, 1, 0, MOUTH[fi], 0 };
}

SlimePet::Visual SlimePet::_thinkVisual() const {
    const int su = SCREEN_H / 10, s_b = su + su / 4;
    int fi = _frame % 10;
    uint8_t r = (fi < 5) ? static_cast<uint8_t>(fi * 3) : 15;
    return { (int)_x, GROUND_Y - s_b, s_b, s_b, 1, -1, 0, 0, r };
}

// ── Renderer ──────────────────────────────────────────────────

void SlimePet::_renderBody(M5Canvas& c, const Visual& v) const {
    int x0 = v.cx - v.bW;
    int y0 = v.cy - v.bH;
    int w  = 2 * v.bW;
    int h  = 2 * v.bH;

    c.fillRect(v.cx - v.bW * 3 / 4, GROUND_Y, v.bW * 3 / 2, 6, Palette::Shadow);
    c.fillRect(x0, y0, w, h, Palette::SlimeBody);
    c.fillRect(x0 + 4, y0 + h * 3 / 4, w - 8, h / 4, Palette::SlimeDark);
    c.fillRect(x0 + 6, y0 + 6, v.bW / 3, v.bH / 3, Palette::SlimeLight);
}

void SlimePet::_renderEyes(M5Canvas& c, const Visual& v) const {
    int eY  = v.cy - v.bH / 3;
    int eLX = v.cx - v.bW / 3;
    int eRX = v.cx + v.bW / 3;

    const int eW = 6;   // eye-white width
    const int eH = 8;   // eye-white height
    const int pW = 4;   // pupil width
    const int pH = 6;   // pupil height

    if (v.eyeState == 2) {
        // Squinting — thin horizontal bar across eye width
        c.fillRect(eLX - eW / 2, eY - 1, eW, 2, Palette::Black);
        c.fillRect(eRX - eW / 2, eY - 1, eW, 2, Palette::Black);
        return;
    }

    if (v.eyeState == 1) {
        // Half-closed — show only bottom half of eye white
        c.fillRect(eLX - eW / 2, eY, eW, eH / 2, Palette::White);
        c.fillRect(eLX - pW / 2, eY + eH / 2 - pW / 2, pW, pW, Palette::Black);

        c.fillRect(eRX - eW / 2, eY, eW, eH / 2, Palette::White);
        c.fillRect(eRX - pW / 2, eY + eH / 2 - pW / 2, pW, pW, Palette::Black);
        return;
    }

    // Open — tall rectangular eye whites (2 wide : 3 tall)
    c.fillRect(eLX - eW / 2, eY - eH / 2, eW, eH, Palette::White);
    c.fillRect(eRX - eW / 2, eY - eH / 2, eW, eH, Palette::White);

    int px = constrain(v.eyeX, -1, 1) * 2;
    int py = constrain(v.eyeY, -1, 1) * 2;
    c.fillRect(eLX - pW / 2 + px, eY - pH / 2 + py, pW, pH, Palette::Black);
    c.fillRect(eRX - pW / 2 + px, eY - pH / 2 + py, pW, pH, Palette::Black);

    // Highlight — 2×2 dot top-right of each pupil
    c.fillRect(eLX + px + pW / 2 - 2, eY - pH / 2 + py, 2, 2, Palette::White);
    c.fillRect(eRX + px + pW / 2 - 2, eY - pH / 2 + py, 2, 2, Palette::White);
}


void SlimePet::_renderThinkBubble(M5Canvas& c, const Visual& v) const {
    int bx = v.cx + v.bW + 20;
    int by = v.cy - v.bH - 12;

    c.fillCircle(v.cx + v.bW / 2 + 4,  v.cy - v.bH / 3,   3, Palette::Bubble);
    c.fillCircle(v.cx + v.bW / 2 + 8,  v.cy - v.bH * 2/3, 3, Palette::Bubble);
    c.fillCircle(v.cx + v.bW / 2 + 14, v.cy - v.bH - 3,   6, Palette::Bubble);
    c.fillCircle(bx, by, v.thinkR, Palette::Bubble);
    c.drawCircle(bx, by, v.thinkR, Palette::BubbleBdr);

    if (v.thinkR >= 12) {
        c.setTextColor(Palette::BubbleTxt, Palette::Bubble);
        c.setTextDatum(lgfx::middle_center);
        c.setTextSize(1);
        c.drawString("?", bx, by);
    }
}

void SlimePet::_renderSpeechBubble(M5Canvas& c, const Visual& v) const {
    if (!speechActive()) return;

    c.setFont(&fonts::Font0);
    c.setTextSize(1);

    const int cx   = v.cx;
    const int topY = v.cy - v.bH;
    const int pad  = 4;
    const int lineH = 9;
    const int maxW  = SCREEN_W - 8 - pad * 2;

    char lines[3][24] = {};
    int  nLines = 0;
    char cur[24] = {};

    for (const char* p = _speech; *p && nLines < 3; ) {
        char word[24] = {};
        int  wi = 0;
        while (*p && *p != ' ' && wi < 23) word[wi++] = *p++;
        if (*p == ' ') ++p;

        char test[48] = {};
        if (cur[0]) snprintf(test, sizeof(test), "%s %s", cur, word);
        else        snprintf(test, sizeof(test), "%s",    word);

        if ((int)c.textWidth(test) <= maxW) {
            strncpy(cur, test, sizeof(cur) - 1);
        } else {
            if (cur[0]) strncpy(lines[nLines++], cur, 23);
            strncpy(cur, word, sizeof(cur) - 1);
        }
    }
    if (cur[0] && nLines < 3) strncpy(lines[nLines++], cur, 23);
    if (nLines == 0) return;

    int textW = 0;
    for (int i = 0; i < nLines; i++) {
        int w = (int)c.textWidth(lines[i]);
        if (w > textW) textW = w;
    }

    int bW = textW + pad * 2 + 2;
    int bH = nLines * lineH + pad * 2 - 1;
    if (bW < 40)            bW = 40;
    if (bW > SCREEN_W - 4)  bW = SCREEN_W - 4;

    int bX = cx - bW / 2;
    int bY = topY - bH - 6;
    if (bX < 2)                 bX = 2;
    if (bX + bW > SCREEN_W - 2) bX = SCREEN_W - bW - 2;
    if (bY < BTN_STRIP + 2)     bY = BTN_STRIP + 2;

    c.fillRect(bX, bY, bW, bH, Palette::Bubble);
    c.drawRect(bX, bY, bW, bH, Palette::BubbleBdr);

    int tailX = cx;
    if (tailX < bX + 2)       tailX = bX + 2;
    if (tailX > bX + bW - 3)  tailX = bX + bW - 3;
    c.fillRect(tailX - 1, bY + bH, 3, 4, Palette::Bubble);

    c.setTextColor(Palette::BubbleTxt, Palette::Bubble);
    c.setTextDatum(lgfx::top_left);
    for (int i = 0; i < nLines; i++) {
        int lw = (int)c.textWidth(lines[i]);
        int lx = bX + (bW - lw) / 2;
        c.drawString(lines[i], lx, bY + pad + i * lineH);
    }
}
