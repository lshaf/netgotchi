#include "App.h"
#include "AppLayout.h"
#include "../net/WebFileServer.h"
#include "Virus.h"
#include "Theme.h"
#include "../core/RandomSeed.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <stdarg.h>
#include "command/ProfileCommand.h"
#include "command/PowerOffCommand.h"
#include "command/CrackCommand.h"
#include "command/NethuntCommand.h"
#include "command/NettrapCommand.h"
#include "command/NetguardCommand.h"
#include "command/SetCommand.h"
#include "command/VaultCommand.h"
#include "command/CleanEapolCommand.h"
#include "command/WebServerCommand.h"

using namespace AppLayout;

static constexpr int SD_CS = 4;

static WebFileServer s_webserver;

NethuntCommand    s_nethunt;
NettrapCommand    s_nettrap;
NetguardCommand   s_netguard;
ProfileCommand    s_profile;
CrackCommand      s_crack;
VaultCommand      s_vault;
SetCommand        s_set;
PowerOffCommand   s_poweroff;
CleanEapolCommand s_cleaneapol;
WebServerCommand  s_wsrv;
MenuCommand*      s_rootItems[] = {
    &s_nethunt, &s_nettrap, &s_netguard, &s_profile, &s_crack, &s_vault, &s_cleaneapol, &s_wsrv, &s_set, &s_poweroff
};
int ROOT_N = (int)(sizeof(s_rootItems) / sizeof(s_rootItems[0]));

// ── IMenuHost ─────────────────────────────────────────────────

void App::cmdPush(const char* text)   { _qPushCmd("%s", text); }
void App::outPush(const char* text)   { _qPushOut("%s", text); }
void App::menuClose() { _menuState = MenuState::Closed; _activeSubCmd = nullptr; _menuScroll = 0; _menuLastSubCount = -1; }
void App::menuBack()  { _menuState = MenuState::Root;   _activeSubCmd = nullptr; _menuScroll = 0; _menuLastSubCount = -1; }
void App::openSubMenu(MenuCommand* cmd) { _activeSubCmd = cmd; _menuState = MenuState::Sub; _menuScroll = 0; _menuLastSubCount = -1; }
void App::setPendingTheme(int8_t idx)          { _pendingTheme      = idx; }
void App::setPendingBrightness(uint8_t v)      { _pendingBright     = (int)v; }
void App::setPendingDisplayOff(uint16_t secs)  { _pendingDisplayOff = (int)secs; }
void App::setPendingPowerOff()                 { _pendingPowerOff   = true; }

void App::_stopCurrentService() {
    if (!_currentService) return;
    _currentService->stopService(*this);
    _currentService->clearState();
    _currentService = nullptr;
    menuClose();
}

void App::startService(MenuCommand* cmd) {
    cmd->startHardware();
    _currentService = cmd;
    _currentService->clearState();
    menuClose();
}

bool App::typingIdle() const { return _qCount == 0 && _typeLen == 0; }
bool App::menuIsOpen() const { return _menuState != MenuState::Closed; }
void App::onCapture()        { _stats.onCapture();  _stats.save(); }
void App::onCracked()        { _stats.onCrack();    _stats.save(); }
void App::onDiscover()       { _stats.onDiscover(); _stats.save(); }
void App::setCurrentService(MenuCommand* cmd) { _currentService = cmd; }

// ── Init ──────────────────────────────────────────────────────

void App::init() {
    RandomSeed::init();

    if (psramFound()) psramInit();

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setBrightness(Theme::brightness());

    _canvas = new M5Canvas(&M5.Display);
    _canvas->setColorDepth(16);
    _canvas->createSprite(SCR_W, SCR_H);

    bool sdOk = SD.begin(SD_CS);
    if (sdOk) {
        SD.mkdir("/netgotchi");
        SD.mkdir("/netgotchi/eapol");
        SD.mkdir("/netgotchi/dictionaries");
    }

    _stats.load();
    Theme::load();
    _hunter.init();
    _hunter.pause();
    s_nethunt.init(&_hunter);
    s_nettrap.init(&_hunter);
    s_netguard.init(&_guard);
    s_profile.init(&_stats);
    s_cleaneapol.init(&_hunter);
    s_webserver.setActivityCallback([this](const char* msg) { _qPushOut("%s", msg); });
    s_webserver.setOnCrackSaved([this]() { onCracked(); });
    s_webserver.begin();

    uint32_t ms = millis();
    _cursorMs    = ms;
    _typeStepMs  = ms;
    _lastTouchMs = ms;

    _qPushCmd("boot netgotchi");
    _qPushOut("net_gotchi term v1.0");
    _qPushOut("psram %ukb free", (unsigned)(ESP.getFreePsram() / 1024));
    if (sdOk) _qPushOut("sd ok %llumb", SD.totalBytes() / (1024 * 1024));
    else      _qPushOut("sd: mount failed");
    _qPushOut("wifi ready.");
    _qPushOut("ready.");


}

// ── Log ring ──────────────────────────────────────────────────

void App::_logPush(const char* line) {
    strncpy(_logBuf[_logHead], line, LINE_COL - 1);
    _logBuf[_logHead][LINE_COL - 1] = '\0';
    _logHead = (_logHead + 1) % LOG_LINES;
}

// ── Queue ─────────────────────────────────────────────────────

void App::_qPushCmd(const char* fmt, ...) {
    if (_qCount >= Q_SIZE) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(_queue[_qTail], LINE_COL, fmt, ap);
    va_end(ap);
    _queueKind[_qTail] = KIND_CMD;
    _qTail = (_qTail + 1) % Q_SIZE;
    _qCount++;
}

void App::_qPushOut(const char* fmt, ...) {
    if (_qCount >= Q_SIZE) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(_queue[_qTail], LINE_COL, fmt, ap);
    va_end(ap);
    _queueKind[_qTail] = KIND_OUT;
    _qTail = (_qTail + 1) % Q_SIZE;
    _qCount++;
}

bool App::_qPop(uint8_t* outKind) {
    if (_qCount == 0) return false;
    strncpy(_typeLine, _queue[_qHead], LINE_COL - 1);
    _typeLine[LINE_COL - 1] = '\0';
    if (outKind) *outKind = _queueKind[_qHead];
    _qHead = (_qHead + 1) % Q_SIZE;
    _qCount--;
    _typeLen = (int)strlen(_typeLine);
    _typeIdx = 0;
    _typeDone = false;
    _typeDoneMs = 0;
    return true;
}

// ── Typing animation ──────────────────────────────────────────

void App::_updateTyping(uint32_t ms) {
    if (ms - _cursorMs >= CURSOR_MS) {
        _cursorMs = ms;
        _cursorOn = !_cursorOn;
    }

    if (_typeLen == 0) {
        uint8_t kind;
        while (_qPop(&kind)) {
            if (kind == KIND_OUT) {
                char buf[LINE_COL + 4];
                snprintf(buf, sizeof(buf), "  %s", _typeLine);
                _logPush(buf);
                _typeLine[0] = '\0';
                _typeLen = 0;
                continue;
            }
            _typeStepMs = ms;
            break;
        }
        return;
    }

    if (!_typeDone) {
        if (ms - _typeStepMs >= TYPE_STEP_MS) {
            _typeStepMs = ms;
            _typeIdx++;
            if (_typeIdx >= _typeLen) {
                _typeIdx    = _typeLen;
                _typeDone   = true;
                _typeDoneMs = ms;
            }
        }
        return;
    }

    if (ms - _typeDoneMs >= TYPE_HOLD_MS) {
        char buf[LINE_COL + 4];
        snprintf(buf, sizeof(buf), "$ %s", _typeLine);
        _logPush(buf);
        _typeLine[0] = '\0';
        _typeLen = _typeIdx = 0;
        _typeDone = false;
        if (_pendingTheme >= 0) {
            Theme::apply(_pendingTheme);
            Theme::save();
            _pendingTheme = -1;
        }
        if (_pendingBright >= 0) {
            Theme::applyBrightness((uint8_t)_pendingBright);
            Theme::save();
            _pendingBright = -1;
        }
        if (_pendingDisplayOff >= 0) {
            Theme::applyDisplayOff((uint16_t)_pendingDisplayOff);
            _pendingDisplayOff = -1;
        }
        if (_pendingPowerOff) {
            _pendingPowerOff = false;
            _menuState = MenuState::PowerWait;
            _powerOffMs = ms;
        }
    }
}

// ── Touch handling ────────────────────────────────────────────

void App::_handleTouch(uint32_t ms) {
    auto t = M5.Touch.getDetail();

    // Reset idle timer on any touch activity
    if (t.isPressed() || t.wasPressed() || t.wasReleased()) {
        _lastTouchMs = ms;
    }

    // Wake display only on a fresh tap — not on the ongoing press that turned it off
    if (t.wasPressed() && _displayOff) {
        M5.Display.setBrightness(Theme::brightness());
        _displayOff = false;
        return;
    }

    if (_menuState == MenuState::PowerWait) return;

    bool pressed  = t.isPressed();
    bool released = t.wasReleased();
    int  tx = t.x, ty = t.y;

    // Display-off icon tap
    if (t.wasPressed() && tx >= DISP_BTN_X && tx < DISP_BTN_X + DISP_BTN_W
            && ty >= DISP_BTN_Y && ty < DISP_BTN_Y + DISP_BTN_H) {
        M5.Display.setBrightness(0);
        _displayOff = true;
        return;
    }

    if (_menuState == MenuState::Closed) {
        if (t.wasPressed() && tx >= Virus::X0 && ty >= 0 && ty < HEADER_DIVIDER_Y) {
            _menuState      = MenuState::Root;
            _menuHighlight  = -1;
            _menuJustOpened = true;
        }
        return;
    }

    if (_menuJustOpened) {
        if (released) _menuJustOpened = false;
        return;
    }

    int nItems = 0, itemH = MENU_ITEM_H;
    if (_menuState == MenuState::Root) {
        nItems = _currentService ? 1 : ROOT_N;
    } else if (_menuState == MenuState::Sub && _activeSubCmd) {
        nItems = _activeSubCmd->subCount();
        itemH  = _activeSubCmd->subItemH();
    }

    if (_menuLastSubCount != nItems) {
        _menuScroll       = 0;
        _menuLastSubCount = nItems;
    }

    int maxVis = (INPUT_DIVIDER_Y - HEADER_DIVIDER_Y - 4) / itemH;
    if (maxVis < 3) maxVis = 3;
    bool paginated     = nItems > maxVis;
    int  slotCount     = paginated ? maxVis + 1 : nItems;
    int  itemsPerPage  = paginated ? maxVis     : nItems;
    int  firstItemSlot = paginated ? 1 : 0;
    if (paginated) {
        int maxScroll = nItems - itemsPerPage;
        if (_menuScroll < 0)         _menuScroll = 0;
        if (_menuScroll > maxScroll) _menuScroll = maxScroll;
    } else {
        _menuScroll = 0;
    }
    int menuTop = INPUT_DIVIDER_Y - slotCount * itemH;

    bool inMenu = (tx >= MARGIN && tx < SCR_W - MARGIN &&
                   ty >= menuTop && ty < INPUT_DIVIDER_Y);
    int hitSlot = -1;
    if (inMenu) {
        hitSlot = (ty - menuTop) / itemH;
        if (hitSlot >= slotCount) hitSlot = -1;
    }

    if (pressed) {
        if (paginated && hitSlot == 0) {
            _navHighlight  = (tx < SCR_W / 2) ? 0 : 1;
            _menuHighlight = -1;
        } else {
            _menuHighlight = (int8_t)hitSlot;
            _navHighlight  = -1;
        }
        return;
    }

    if (!released) return;

    _menuHighlight = -1;
    _navHighlight  = -1;

    if (!inMenu || hitSlot < 0) {
        menuClose();
        return;
    }

    int sel = hitSlot;

    if (paginated && sel == 0) {
        if (tx < SCR_W / 2) {
            _menuScroll -= itemsPerPage;
            if (_menuScroll < 0) _menuScroll = 0;
        } else {
            _menuScroll += itemsPerPage;
            int maxScroll = nItems - itemsPerPage;
            if (_menuScroll > maxScroll) _menuScroll = maxScroll;
        }
        return;
    }

    int itemIdx = _menuScroll + (sel - firstItemSlot);
    if (itemIdx < 0 || itemIdx >= nItems) {
        menuClose();
        return;
    }

    if (_menuState == MenuState::Root) {
        if (_currentService) { _stopCurrentService(); return; }
        s_rootItems[itemIdx]->execute(*this);
        return;
    }

    if (_menuState == MenuState::Sub && _activeSubCmd) {
        _activeSubCmd->onSubSelect(*this, itemIdx);
        return;
    }
}

// ── Main loop ─────────────────────────────────────────────────

void App::update() {
    M5.update();
    uint32_t ms = millis();

    if (_menuState == MenuState::PowerWait && ms - _powerOffMs >= 2000) {
        _stats.save();
        M5.Display.fillScreen(0);
        M5.Power.powerOff();
        while (true) { delay(1000); }
    }

    uint16_t offSecs = Theme::displayOffSecs();
    if (offSecs > 0 && !_displayOff) {
        if (ms - _lastTouchMs >= (uint32_t)offSecs * 1000u) {
            M5.Display.setBrightness(0);
            _displayOff = true;
        }
    }


    _handleTouch(ms);
    if (_currentService)
        _currentService->update(*this, ms);
    _updateTyping(ms);

    if (!_displayOff) {
        _canvas->fillScreen(Theme::BG);
        _drawHud(*_canvas, ms);
        _drawLog(*_canvas);
        if (_menuState == MenuState::Root || _menuState == MenuState::Sub) {
            _drawMenuContent(*_canvas);
        } else {
            _drawInput(*_canvas);
        }
        _canvas->pushSprite(0, 0);
    }
}
