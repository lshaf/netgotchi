#pragma once
#include "../net/WiFiHunter.h"
#include "Stats.h"
#include <M5GFX.h>

class App {
public:
    void init();
    void update();

private:
    static constexpr int LINE_COL  = 53;   // chars + null  (52 visible @ 6px = 312px)
    static constexpr int LOG_LINES = 20;
    static constexpr int Q_SIZE    = 16;

    static constexpr uint8_t KIND_CMD = 0;
    static constexpr uint8_t KIND_OUT = 1;

    M5Canvas*  _canvas = nullptr;
    WiFiHunter _hunter;
    Stats      _stats;

    // ── Scrollback ring ──────────────────────────────────────────
    char    _logBuf[LOG_LINES][LINE_COL] = {};
    uint8_t _logHead = 0;     // next write slot

    // ── Type-out queue ───────────────────────────────────────────
    char    _queue[Q_SIZE][LINE_COL] = {};
    uint8_t _queueKind[Q_SIZE]       = {};
    uint8_t _qHead = 0, _qTail = 0, _qCount = 0;

    // ── Currently typing line (commands only) ─────────────────────
    char     _typeLine[LINE_COL] = {};
    int      _typeLen   = 0;
    int      _typeIdx   = 0;
    uint32_t _typeStepMs = 0;
    bool     _typeDone   = false;
    uint32_t _typeDoneMs = 0;

    // ── Cursor blink ─────────────────────────────────────────────
    bool     _cursorOn = true;
    uint32_t _cursorMs = 0;

    // ── Hunting state tracking ───────────────────────────────────
    uint32_t _lastCaptureCount          = 0;
    uint32_t _lastApFoundCount          = 0;
    uint32_t _lastDeauthTargetCount     = 0;
    uint32_t _lastEapolEventCount       = 0;
    uint32_t _statusLogMs               = 0;
    uint8_t  _lastChannel               = 0;
    uint32_t _pauseUntilMs              = 0;
    uint8_t  _exhaustPhase              = 0;  // 0=normal 1=60s wait 2=5s start wait

    // ── Menu overlay ─────────────────────────────────────────────
    bool _menuOpen = false;
    int  _menuTab  = 0;   // 0=profile  1=setting

    void _logPush  (const char* line);
    void _qPushCmd (const char* fmt, ...);
    void _qPushOut (const char* fmt, ...);
    bool _qPop     (uint8_t* outKind);

    void _updateTyping (uint32_t ms);
    void _updateHunting(uint32_t ms);
    void _handleTouch  (uint32_t ms);

    void _drawHud   (M5Canvas& c, uint32_t ms) const;
    void _drawLog   (M5Canvas& c) const;
    void _drawInput (M5Canvas& c) const;
    void _drawMenu  (M5Canvas& c) const;
};
