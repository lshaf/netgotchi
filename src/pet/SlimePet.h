#pragma once
#include "../core/types.h"
#include <cstdint>
#include <M5GFX.h>

class SlimePet {
public:
    SlimePet();

    void setAnim(Anim a);
    void setSpeech(const char* msg);   // show speech bubble for ~3 s; nullptr clears
    void setStayRight(bool v)  { _stayRight = v; if (v) _walkDir = 1; }
    void setPinCenter(bool v)  { _pinCenter = v; }
    void update(uint32_t ms);
    void draw(M5Canvas& c) const;

    Anim  currentAnim()  const { return _anim; }
    float posX()         const { return _x;    }
    bool  speechActive() const {
        return _speech && _speechStart > 0 && (_nowMs - _speechStart) < SPEECH_DUR;
    }

private:
    struct Visual {
        int     cx, cy;
        int     bW, bH;
        int     eyeX, eyeY;
        uint8_t eyeState;
        uint8_t mouthH;
        uint8_t thinkR;
    };

    static constexpr uint32_t SPEECH_DUR = 3000;

    // ── Animation state ───────────────────────────────────────
    Anim     _anim      = Anim::Idle;
    int      _frame     = 0;
    uint32_t _lastMs    = 0;
    float    _x            = SCREEN_W / 2.0f;
    int      _walkDir      = 1;
    bool     _stayRight    = false;
    bool     _pinCenter    = false;
    uint32_t _wallPauseEnd = 0;

    // ── Speech bubble ─────────────────────────────────────────
    mutable const char* _speech      = nullptr;   // mutable: cleared on expiry in draw()
    uint32_t            _speechStart = 0;
    uint32_t            _nowMs       = 0;         // updated every tick, used for bubble timer

    // ── Frame generators ──────────────────────────────────────
    Visual _idleVisual()  const;
    Visual _walkVisual()  const;
    Visual _talkVisual()  const;
    Visual _thinkVisual() const;
    Visual _currentVisual() const;

    // ── Render helpers ────────────────────────────────────────
    void _renderBody        (M5Canvas& c, const Visual& v) const;
    void _renderEyes        (M5Canvas& c, const Visual& v) const;
    void _renderThinkBubble (M5Canvas& c, const Visual& v) const;
    void _renderSpeechBubble(M5Canvas& c, const Visual& v) const;
};
