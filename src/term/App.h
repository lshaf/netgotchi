#pragma once
#include "../net/WiFiHunter.h"
#include "Stats.h"
#include "command/MenuCommand.h"
#include <M5GFX.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

struct CrackHandshake {
    char     ssid[33]     = {};
    uint8_t  ssid_len     = 0;
    uint8_t  ap[6]        = {};
    uint8_t  sta[6]       = {};
    uint8_t  anonce[32]   = {};
    uint8_t  snonce[32]   = {};
    uint8_t  mic[16]      = {};
    uint8_t  eapol[300]   = {};
    uint16_t eapol_len    = 0;
    uint8_t  prf_data[76] = {};
};

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
    void setPendingPowerOff() override;
    void     startCrack(const char* pcapPath, const char* dictPath) override;
    void     startNetgotchi() override;
    uint32_t statsXp()         const override;
    uint32_t statsCaptures()   const override;
    uint32_t statsLevel()      const override;
    uint32_t statsXpProgress() const override;
    int      statsBattery()    const override;
    bool     statsCharging()   const override;

private:
    static constexpr int LINE_COL  = 53;
    static constexpr int LOG_LINES = 20;
    static constexpr int Q_SIZE    = 16;

    static constexpr uint8_t KIND_CMD = 0;
    static constexpr uint8_t KIND_OUT = 1;

    M5Canvas*  _canvas = nullptr;
    WiFiHunter _hunter;
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

    // ── Hunting state tracking ───────────────────────────────────
    uint32_t _lastCaptureCount      = 0;
    uint32_t _lastApFoundCount      = 0;
    uint32_t _lastDeauthTargetCount = 0;
    uint32_t _lastEapolEventCount   = 0;
    uint32_t _statusLogMs           = 0;
    uint8_t  _lastChannel           = 0;
    uint32_t _pauseUntilMs          = 0;
    uint8_t  _exhaustPhase          = 0;

    // ── Menu state ────────────────────────────────────────────────
    enum class MenuState : uint8_t { Closed, Root, Sub, PowerWait };
    MenuState    _menuState       = MenuState::Closed;
    int8_t       _pendingTheme    = -1;
    int          _pendingBright   = -1;
    bool         _pendingPowerOff = false;
    uint32_t     _powerOffMs      = 0;

    MenuCommand* _activeSubCmd    = nullptr;  // command owning the open sub-menu

    int8_t       _menuHighlight   = -1;
    bool         _menuJustOpened  = false;
    int          _menuScroll      = 0;
    int          _menuLastSubCount = -1;

    // ── Crack state ───────────────────────────────────────────────
    static constexpr int CRACK_QUEUE_DEPTH = 8;
    static constexpr int CRACK_PASS_MAX    = 64;

    struct CrackPwEntry { char pw[CRACK_PASS_MAX]; uint8_t len; };

    struct CrackCtx {
        CrackHandshake    hs;
        char              wordlistPath[64] = {};
        QueueHandle_t     queue        = nullptr;
        SemaphoreHandle_t doneSem      = nullptr;
        TaskHandle_t      workerHandle = nullptr;
        volatile bool     stop     = false;
        volatile bool     done     = false;
        volatile bool     found    = false;
        char              foundPass[64]  = {};
        char              curPass[64]    = {};
        volatile uint32_t tested    = 0;
        volatile uint32_t bytesDone = 0;
        volatile uint32_t fileSize  = 0;
    };

    enum class CrackState : uint8_t { Idle, Running };
    CrackState   _crackState      = CrackState::Idle;
    CrackCtx     _crackCtx        = {};
    TaskHandle_t _crackProdHandle = nullptr;
    char         _crackPcapPath[64] = {};
    bool         _pendingCrack    = false;
    uint32_t     _crackStartMs    = 0;

    // ── Netgotchi (hunter) state ──────────────────────────────────
    bool         _netgotchiRunning = false;
    void         _stopNetgotchi();
    void         _stopCrack();

    void _startCrack    ();
    void _updateCracking(uint32_t ms);
    static void _crackWorkerTask(void* param);
    static void _crackProdTask  (void* param);

    void _logPush  (const char* line);
    void _qPushCmd (const char* fmt, ...);
    void _qPushOut (const char* fmt, ...);
    bool _qPop     (uint8_t* outKind);

    void _updateTyping  (uint32_t ms);
    void _updateHunting (uint32_t ms);
    void _handleTouch   (uint32_t ms);

    void _drawHud         (M5Canvas& c, uint32_t ms) const;
    void _drawLog         (M5Canvas& c) const;
    void _drawInput       (M5Canvas& c) const;
    void _drawMenuContent (M5Canvas& c) const;
};
