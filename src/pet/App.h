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
    M5Canvas* _canvas = nullptr;

    Background _bg;
    SlimePet   _pet;
    WiFiHunter _hunter;
    PetStats   _stats;

    // ── Hunt state machine ────────────────────────────────────
    enum class HuntState { Idle=0, PreAttack=1, Attacking=2, Captured=3, Done=4, Menu=5 };
    HuntState _huntState    = HuntState::Idle;
    uint32_t  _stateEntryMs = 0;
    uint32_t  _quipMs       = 0;
    char      _speechBuf[24] = {};

    // ── Capture tracking (mirrors WiFiHunter::captureCount) ───
    uint32_t _lastCaptureCount = 0;
    int      _lastChannel      = -1;

    // ── Menu overlay ──────────────────────────────────────────
    bool      _menuReady    = false;

    // ── Serial status log ─────────────────────────────────────
    uint32_t _lastStatusLogMs = 0;

    // ── Scrolling terminal log ────────────────────────────────
    static constexpr int TERM_LINES = 24;
    static constexpr int TERM_COL   = 24;  // 23 chars + null
    char    _termBuf[TERM_LINES][TERM_COL] = {};
    uint8_t _termHead = 0;

    void _applyHuntState(HuntState next, uint32_t ms);
    void _updateHunting(uint32_t ms);
    void _onCapture(uint32_t ms);
    void _handleTouch(uint32_t ms);
    void _termPush(const char* line);
    void _drawHud     (M5Canvas& c) const;
    void _drawTerminal(M5Canvas& c) const;
    void _drawMenu    (M5Canvas& c) const;
};
