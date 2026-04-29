#pragma once
#include "../net/WiFiHunter.h"
#include "../net/WifiGuard.h"
#include "Stats.h"
#include "command/MenuCommand.h"
#include <M5GFX.h>

class App : public IMenuHost {
public:
    void init();
    void update();

    // IMenuHost interface
    void cmdPush(const char* text) override;
    void outPush(const char* text) override;
    void menuClose() override;
    void menuBack()  override;
    void openSubMenu(MenuCommand* cmd) override;
    void setPendingTheme(int8_t idx) override;
    void setPendingBrightness(uint8_t val255) override;
    void setPendingDisplayOff(uint16_t secs) override;
    void setPendingPowerOff() override;
    bool     typingIdle()    const override;
    bool     menuIsOpen()    const override;
    void startService(MenuCommand* cmd)      override;
    void setCurrentService(MenuCommand* cmd) override;
    void onCapture()  override;
    void onCracked()  override;
    void onDiscover() override;

private:
    static constexpr int LINE_COL  = 53;
    static constexpr int LOG_LINES = 20;
    static constexpr int Q_SIZE    = 16;

    static constexpr uint8_t KIND_CMD = 0;
    static constexpr uint8_t KIND_OUT = 1;

    M5Canvas*  _canvas = nullptr;
    WiFiHunter _hunter;
    WifiGuard  _guard;
    Stats      _stats;

    // ── Scrollback ring ──────────────────────────────────────────
    char    _logBuf[LOG_LINES][LINE_COL] = {};
    uint8_t _logHead = 0;

    // ── Type-out queue ───────────────────────────────────────────
    char    _queue[Q_SIZE][LINE_COL] = {};
    uint8_t _queueKind[Q_SIZE]       = {};
    uint8_t _qHead = 0, _qTail = 0, _qCount = 0;

    // ── Currently typing line ────────────────────────────────────
    char     _typeLine[LINE_COL] = {};
    int      _typeLen   = 0;
    int      _typeIdx   = 0;
    uint32_t _typeStepMs = 0;
    bool     _typeDone   = false;
    uint32_t _typeDoneMs = 0;

    // ── Cursor blink ─────────────────────────────────────────────
    bool     _cursorOn = true;
    uint32_t _cursorMs = 0;

    // ── Menu state ────────────────────────────────────────────────
    enum class MenuState : uint8_t { Closed, Root, Sub, PowerWait };
    MenuState    _menuState       = MenuState::Closed;
    int8_t       _pendingTheme      = -1;
    int          _pendingBright     = -1;
    int          _pendingDisplayOff = -1;
    bool         _pendingPowerOff   = false;

    // ── Display-off state ─────────────────────────────────────────
    uint32_t     _lastTouchMs  = 0;
    bool         _displayOff   = false;
    uint32_t     _powerOffMs   = 0;

    MenuCommand* _activeSubCmd    = nullptr;

    int8_t       _menuHighlight   = -1;
    int8_t       _navHighlight    = -1;
    bool         _menuJustOpened  = false;
    int          _menuScroll      = 0;
    int          _menuLastSubCount = -1;

    // ── Active service (only one runs at a time) ──────────────────
    MenuCommand* _currentService = nullptr;
    void         _stopCurrentService();

    void _logPush  (const char* line);
    void _qPushCmd (const char* fmt, ...);
    void _qPushOut (const char* fmt, ...);
    bool _qPop     (uint8_t* outKind);

    void _updateTyping  (uint32_t ms);
    void _handleTouch   (uint32_t ms);

    void _drawHud         (M5Canvas& c, uint32_t ms) const;
    void _drawLog         (M5Canvas& c) const;
    void _drawInput       (M5Canvas& c) const;
    void _drawMenuContent (M5Canvas& c) const;
};
