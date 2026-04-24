#pragma once
#include "Background.h"
#include "SlimePet.h"
#include "PetStats.h"
#include "../net/WiFiHunter.h"
#include <M5GFX.h>

class App {
public:
    void init();
    void update();

private:
    M5Canvas* _canvas      = nullptr;
    M5Canvas* _pixelCanvas = nullptr;

    Background _bg;
    SlimePet   _pet;
    WiFiHunter _hunter;
    PetStats   _stats;

    // ── Hunt state machine ────────────────────────────────────
    enum class HuntState { Idle=0, Listing=1, Deauthing=2, Waiting=3, Captured=4, Taunting=5 };
    HuntState _huntState    = HuntState::Idle;
    uint32_t  _stateEntryMs = 0;
    uint32_t  _quipMs       = 0;
    char      _speechBuf[16] = {};

    // ── Capture tracking (mirrors WiFiHunter::captureCount) ───
    uint32_t _lastCaptureCount = 0;
    uint8_t  _lastApCount      = 0;

    // ── Zero-result sweep taunt ───────────────────────────────
    uint8_t  _zeroSweepCount  = 0;
    uint8_t  _zeroSweepThresh = 2;

    // ── Serial status log ─────────────────────────────────────
    uint32_t _lastStatusLogMs = 0;

    // ── Scrolling terminal log ────────────────────────────────
    static constexpr int TERM_LINES = 10;
    char    _termBuf[TERM_LINES][14] = {};  // 13 chars + null (fits 80px virtual canvas)
    uint8_t _termHead   = 0;
    uint8_t _lastTermCh = 0;

    void _applyHuntState(HuntState next, uint32_t ms);
    void _updateHunting(uint32_t ms);
    void _onCapture(uint32_t ms);
    void _termPush(const char* line);
    void _drawHud     (M5Canvas& c) const;
    void _drawTerminal(M5Canvas& c) const;
};
