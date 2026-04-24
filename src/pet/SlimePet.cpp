#include "SlimePet.h"
#include "../core/Palette.h"
#include <M5Unified.h>
#include <cmath>
#include <cstring>

static constexpr uint16_t FRAME_MS[] = { 100, 115, 250, 310 };
static constexpr uint32_t SPEECH_DUR = 3000;  // ms a speech bubble stays up

// ── Construction ──────────────────────────────────────────────

SlimePet::SlimePet() : _x(SCREEN_W / 2.0f) {}

// ── Public interface ──────────────────────────────────────────

void SlimePet::setAnim(Anim a) {
    if (_anim == a) return;
    _anim   = a;
    _frame  = 0;
    _lastMs = 0;
}

void SlimePet::setSpeech(const char* msg) {
    _speech      = msg;
    _speechStart = 0;  // re-arm: will be set to _nowMs on next update tick
}

void SlimePet::update(uint32_t ms) {
    _nowMs = ms;
    if (_speech && _speechStart == 0) _speechStart = ms;

    uint32_t iv = FRAME_MS[static_cast<uint8_t>(_anim)];
    if (ms - _lastMs < iv) return;
    _lastMs = ms;
    _frame++;

    if (_anim == Anim::Walk) {
        _x += _walkDir * 0.7f;
        if (_x > SCREEN_W * 3 / 4) {
            _x = SCREEN_W * 3 / 4;
            if (!_stayRight) _walkDir = -1;
        }
        if (_x < SCREEN_W / 4) { _x = SCREEN_W / 4; _walkDir = 1; }
    } else {
        float dx = SCREEN_W / 2.0f - _x;
        if (fabsf(dx) > 1.5f) _x += dx * 0.12f;
    }
}

void SlimePet::draw(M5Canvas& c) const {
    Visual v = _currentVisual();
    _renderBody(c, v);
    _renderEyes(c, v);
    _renderMouth(c, v);
    if (v.thinkR > 0) _renderThinkBubble(c, v);
    if (_speech && _speechStart > 0 && (_nowMs - _speechStart) < SPEECH_DUR)
        _renderSpeechBubble(c, v);
    else if (_speech && _speechStart > 0)
        _speech = nullptr;  // expired
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
    const int su = SCREEN_H / 7, s_b = su + su / 4;
    static const int8_t YO[] = { 0, 0,-1,-1, 0, 0, 1, 1 };
    int fi = _frame % 8;
    uint8_t eyeState = (fi == 5) ? 1 : (fi == 6) ? 2 : 0;
    return { (int)_x, GROUND_Y - s_b + YO[fi], s_b, s_b, 1, 1, eyeState, 0, 0 };
}

SlimePet::Visual SlimePet::_walkVisual() const {
    const int su = SCREEN_H / 7, s_b = su + su / 4;
    static const int8_t YF[] = { 0,-1,-3,-1, 0,-1,-3,-1 };
    int fi = _frame % 8;
    int yo = YF[fi] * su / 3;
    return { (int)_x, GROUND_Y - s_b + yo, s_b, s_b, _walkDir, 1, 0, 0, 0 };
}

SlimePet::Visual SlimePet::_talkVisual() const {
    const int su = SCREEN_H / 7, s_b = su + su / 4;
    static const uint8_t MOUTH[] = { 0, 1, 2, 2, 1, 0 };
    static const int8_t  YO[]    = { 0, 0,-1,-1, 0, 0 };
    int fi = _frame % 6;
    return { (int)_x, GROUND_Y - s_b + YO[fi], s_b, s_b, 1, 1, 0, MOUTH[fi], 0 };
}

SlimePet::Visual SlimePet::_thinkVisual() const {
    const int su = SCREEN_H / 7, s_b = su + su / 4;
    int fi = _frame % 10;
    uint8_t r = (fi < 5) ? static_cast<uint8_t>(fi) : 5;
    return { (int)_x, GROUND_Y - s_b, s_b, s_b, 1, -1, 0, 0, r };
}

// ── Renderer ──────────────────────────────────────────────────

void SlimePet::_renderBody(M5Canvas& c, const Visual& v) const {
    int x0 = v.cx - v.bW;
    int y0 = v.cy - v.bH;
    int w  = 2 * v.bW;
    int h  = 2 * v.bH;

    c.fillRect(v.cx - v.bW * 3 / 4, GROUND_Y, v.bW * 3 / 2, 2, Palette::Shadow);
    c.fillRect(x0, y0, w, h, Palette::SlimeBody);
    c.fillRect(x0 + 1, y0 + h * 3 / 4, w - 2, h / 4, Palette::SlimeDark);
    c.fillRect(x0 + 2, y0 + 2, v.bW / 3, v.bH / 3, Palette::SlimeLight);
}

void SlimePet::_renderEyes(M5Canvas& c, const Visual& v) const {
    int eY  = v.cy - v.bH / 3;
    int eLX = v.cx - v.bW / 3;
    int eRX = v.cx + v.bW / 3;
    int eS  = SCREEN_H / 14;  // == SU / 2

    if (v.eyeState == 2) {
        c.fillRect(eLX - eS, eY, 2 * eS, 1, Palette::Black);
        c.fillRect(eRX - eS, eY, 2 * eS, 1, Palette::Black);
        return;
    }

    int eH = (v.eyeState == 1) ? 1 : eS;
    c.fillRect(eLX - eS, eY - eH / 2, 2 * eS, eH, Palette::White);
    c.fillRect(eRX - eS, eY - eH / 2, 2 * eS, eH, Palette::White);

    int px = constrain(v.eyeX, -1, 1);
    int py = constrain(v.eyeY, -1, 1);
    int pS = (eS >= 3) ? 2 : 1;
    c.fillRect(eLX + px - pS / 2, eY + py - pS / 2, pS, pS, Palette::Black);
    c.fillRect(eRX + px - pS / 2, eY + py - pS / 2, pS, pS, Palette::Black);
    c.drawPixel(eLX + px + pS / 2, eY + py - pS / 2, Palette::White);
    c.drawPixel(eRX + px + pS / 2, eY + py - pS / 2, Palette::White);
}

void SlimePet::_renderMouth(M5Canvas& c, const Visual& v) const {
    int mY  = v.cy + v.bH / 3;
    int mW  = v.bW;
    int mX0 = v.cx - mW / 2;

    switch (v.mouthH) {
        case 0:
            c.fillRect(mX0, mY, mW, 2, Palette::Black);
            break;
        case 1:
            c.fillRect(mX0 + mW / 4, mY, mW / 2, 3, Palette::Black);
            break;
        default:
            c.fillRect(mX0, mY, mW, 4, Palette::Black);
            c.fillRect(mX0 + 1,      mY, 2, 2, Palette::White);
            c.fillRect(mX0 + mW / 2, mY, 2, 2, Palette::White);
            break;
    }
}

void SlimePet::_renderThinkBubble(M5Canvas& c, const Visual& v) const {
    int bx = v.cx + v.bW + 5;
    int by = v.cy - v.bH - 4;

    c.fillCircle(v.cx + v.bW / 2 + 1, v.cy - v.bH / 3,   1, Palette::Bubble);
    c.fillCircle(v.cx + v.bW / 2 + 2, v.cy - v.bH * 2/3, 1, Palette::Bubble);
    c.fillCircle(v.cx + v.bW / 2 + 4, v.cy - v.bH - 1,   2, Palette::Bubble);
    c.fillCircle(bx, by, v.thinkR, Palette::Bubble);
    c.drawCircle(bx, by, v.thinkR, Palette::BubbleBdr);

    if (v.thinkR >= 4) {
        c.setTextColor(Palette::BubbleTxt, Palette::Bubble);
        c.setTextDatum(lgfx::middle_center);
        c.setTextSize(1);
        c.drawString("?", bx, by);
    }
}

void SlimePet::_renderSpeechBubble(M5Canvas& c, const Visual& v) const {
    if (!_speech) return;

    // Bubble sits above the body with a small tail pointing down-center
    // Width is dynamic: measured from text so longer messages fit naturally
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    int bH   = 10;
    int bW   = (int)c.textWidth(_speech) + 8;
    if (bW > SCREEN_W - 2) bW = SCREEN_W - 2;
    if (bW < 28)            bW = 28;
    int bX   = v.cx - bW / 2;
    int bY   = v.cy - v.bH - bH - 4;

    // Clamp to stay inside canvas (above BTN_STRIP)
    if (bX < 1) bX = 1;
    if (bX + bW > SCREEN_W - 1) bX = SCREEN_W - bW - 1;
    if (bY < BTN_STRIP + 1) bY = BTN_STRIP + 1;

    // Background + border
    c.fillRect(bX, bY, bW, bH, Palette::Bubble);
    c.drawRect(bX, bY, bW, bH, Palette::BubbleBdr);

    // Tail: 2-pixel notch at bottom-center pointing toward slime
    int tailX = v.cx - 1;
    if (tailX < bX + 1) tailX = bX + 1;
    if (tailX > bX + bW - 3) tailX = bX + bW - 3;
    c.fillRect(tailX, bY + bH, 2, 2, Palette::Bubble);

    // Text (font+size already set above for textWidth measurement)
    c.setTextDatum(lgfx::middle_center);
    c.setTextColor(Palette::BubbleTxt, Palette::Bubble);
    c.drawString(_speech, bX + bW / 2, bY + bH / 2);
}
