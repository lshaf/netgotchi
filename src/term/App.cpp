#include "App.h"
#include "AppLayout.h"
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

using namespace AppLayout;

#if defined(ARDUINO_M5STACK_CORES3)
    static constexpr int SD_CS = 4;
#elif defined(ARDUINO_M5STACK_CARDPUTER)
    static constexpr int SD_CS = 12;
#else
    static constexpr int SD_CS = 4;
#endif

NethuntCommand    s_nethunt;
NettrapCommand    s_nettrap;
NetguardCommand   s_netguard;
ProfileCommand    s_profile;
CrackCommand      s_crack;
VaultCommand      s_vault;
SetCommand        s_set;
PowerOffCommand   s_poweroff;
MenuCommand*      s_rootItems[] = {
    &s_nethunt, &s_nettrap, &s_netguard, &s_profile, &s_crack, &s_vault, &s_set, &s_poweroff
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

void App::startNethunt() {
    _guard.pause();
    _hunter.setTrapMode(false);
    _hunter.resume();
    _nethuntRunning = true;
    _qPushCmd("service nethunt start");
    menuClose();
}
void App::_stopNethunt() {
    _hunter.pause();
    _nethuntRunning = false;
    _exhaustPhase   = 0;
    _qPushCmd("service nethunt stop");
    menuClose();
}
void App::startNettrap() {
    _guard.pause();
    _hunter.setTrapMode(true);
    _hunter.resume();
    _nettrapRunning = true;
    _qPushCmd("service nettrap start");
    menuClose();
}
void App::_stopNettrap() {
    _hunter.pause();
    _hunter.setTrapMode(false);
    _nettrapRunning = false;
    _qPushCmd("service nettrap stop");
    menuClose();
}
void App::startNetguard() {
    _hunter.pause();
    _guard.init();
    _netguardRunning = true;
    _lastGuardDeauthCount = 0;
    _lastBeaconFloodCount = 0;
    _lastEvilTwinCount    = 0;
    _qPushCmd("service netguard start");
    menuClose();
}
void App::_stopNetguard() {
    _guard.pause();
    _netguardRunning = false;
    _qPushCmd("service netguard stop");
    menuClose();
}

bool     App::typingIdle()            const { return _qCount == 0 && _typeLen == 0; }
uint32_t App::statsXp()              const { return _stats.xp(); }
uint32_t App::statsCaptures()        const { return _stats.captures(); }
uint32_t App::statsCracked()         const { return _stats.cracked(); }
uint32_t App::statsLevel()           const { return _stats.level(); }
uint32_t App::statsXpProgress()      const { return _stats.xpProgress(); }
int      App::statsBattery()         const { return _stats.battery(); }
bool     App::statsCharging()        const { return _stats.isCharging(); }
void     App::onCracked()                  { _stats.onCrack(); _stats.save(); }
uint32_t App::statsDeauthDiscovers() const { return _stats.deauthDiscovers(); }
uint32_t App::statsFloodDiscovers()  const { return _stats.floodDiscovers(); }
uint32_t App::statsEvilDiscovers()   const { return _stats.evilDiscovers(); }
void     App::onDeauthDiscover()           { _stats.onDeauthDiscover(); _stats.save(); }
void     App::onFloodDiscover()            { _stats.onFloodDiscover(); _stats.save(); }
void     App::onEvilDiscover()             { _stats.onEvilDiscover(); _stats.save(); }

// ── Init ──────────────────────────────────────────────────────

void App::init() {
    Serial.begin(115200);
    RandomSeed::init();

    if (psramFound()) {
        psramInit();
        Serial.printf("[INIT] PSRAM %uKB free\n", ESP.getFreePsram() / 1024);
    }

    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setBrightness(Theme::brightness());

    _canvas = new M5Canvas(&M5.Display);
    _canvas->setColorDepth(16);
    _canvas->createSprite(SCR_W, SCR_H);

    bool sdOk = SD.begin(SD_CS);
    if (sdOk) {
        Serial.printf("[SD] OK  %lluMB total\n", SD.totalBytes() / (1024 * 1024));
        SD.mkdir("/netgotchi");
        SD.mkdir("/netgotchi/eapol");
        SD.mkdir("/netgotchi/dictionaries");
    } else {
        Serial.println("[SD] Mount failed — captures won't be saved");
    }

    _stats.load();
    Theme::load();
    _hunter.init();
    _hunter.pause();

    uint32_t ms = millis();
    _cursorMs    = ms;
    _typeStepMs  = ms;
    _lastTouchMs = ms;

    _qPushCmd("boot netgotchi");
    _qPushOut("net_gotchi term v0.1");
    _qPushOut("psram %ukb free", (unsigned)(ESP.getFreePsram() / 1024));
    if (sdOk) _qPushOut("sd ok %llumb", SD.totalBytes() / (1024 * 1024));
    else      _qPushOut("sd: mount failed");
    _qPushOut("wifi ready.");
    _qPushOut("ready.");

    Serial.println("[INIT] Term boot complete");
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

// ── Hunting integration ───────────────────────────────────────

void App::_updateHunting(uint32_t ms) {
    if (!_nethuntRunning && !_nettrapRunning && !_netguardRunning) return;
    if (s_crack.isRunning()) return;
    if (ms - _statusLogMs >= 10000) {
        _statusLogMs = ms;
        uint8_t ch = (_nethuntRunning || _nettrapRunning) ? _hunter.channel() : _guard.channel();
        const char* modeStr = _nethuntRunning ? "hunt" : (_nettrapRunning ? "trap" : "guard");
        Serial.printf("[STATUS] mode=%s ch=%d bat=%d%% caps=%lu xp=%lu\n",
                      modeStr, ch, M5.Power.getBatteryLevel(),
                      (unsigned long)_stats.captures(), (unsigned long)_stats.xp());
    }

    if (_menuState != MenuState::Closed) return;

    if (_nethuntRunning) {
        if (_exhaustPhase == 1) {
            if (ms < _pauseUntilMs) return;
            _qPushCmd("service nethunt start");
            _pauseUntilMs = ms + 5000;
            _exhaustPhase = 2;
            return;
        }
        if (_exhaustPhase == 2) {
            if (ms < _pauseUntilMs) return;
            _qPushCmd("setchannel 1");
            _exhaustPhase = 0;
        }

        _hunter.update(ms);

        uint8_t ch = _hunter.channel();
        if (ch != _lastChannel) {
            if (_lastChannel == 13 && ch == 1) {
                _hunter.clearFindings(ms);
                _lastApFoundCount      = 0;
                _lastDeauthTargetCount = 0;
                _lastEapolEventCount   = 0;
                _lastCaptureCount      = 0;
                _lastChannel = ch;
                _qPushCmd("service nethunt exhaust 60");
                _pauseUntilMs = ms + 60000;
                _exhaustPhase = 1;
                return;
            }
            _lastChannel = ch;
            _qPushCmd("setchannel %d", ch);
        }

        uint32_t afc = _hunter.apFoundCount();
        if (afc > _lastApFoundCount) {
            _lastApFoundCount = afc;
            const char* ssid = _hunter.lastFoundSsid();
            _qPushOut("detected %.32s", (ssid && ssid[0]) ? ssid : "<hidden>");
        }

        uint32_t dtc = _hunter.deauthTargetCount();
        if (dtc > _lastDeauthTargetCount) {
            _lastDeauthTargetCount = dtc;
            const char* dsid = _hunter.lastDeauthSsid();
            _qPushCmd("deauth %.32s", (dsid && dsid[0]) ? dsid : "??");
        }

        uint32_t eec = _hunter.eapolEventCount();
        if (eec > _lastEapolEventCount) {
            _lastEapolEventCount = eec;
            int msg = _hunter.lastEapolMsg();
            const char* esid = _hunter.lastEapolSsid();
            _qPushOut("traced eapol M%d %.32s", msg, (esid && esid[0]) ? esid : "??");
        }

        uint32_t edc = _hunter.externalDeauthCount();
        if (edc > _lastExternalDeauthCount) {
            _lastExternalDeauthCount = edc;
            const char* eid = _hunter.lastExternalDeauthSsid();
            _qPushOut("alert deauth %.32s", (eid && eid[0]) ? eid : "??");
        }

        uint32_t caps = _hunter.captureCount();
        if (caps > _lastCaptureCount) {
            _lastCaptureCount = caps;
            const char* path  = _hunter.lastCapturePath();
            const char* fname = strrchr(path, '/');
            fname = fname ? fname + 1 : path;
            _stats.onCapture();
            _stats.save();
            uint16_t r1 = 0x1000 + (uint16_t)(rand() & 0xCFFF);
            uint16_t r2 = r1 + (uint16_t)(rand() & 0x0FFF) + 0x100;
            _qPushCmd("dump 0x%04x..0x%04x >> %.27s", r1, r2, fname);
        }
        return;
    }

    if (_nettrapRunning) {
        _hunter.update(ms);

        uint8_t ch = _hunter.channel();
        if (ch != _lastChannel) {
            _lastChannel = ch;
            _qPushCmd("setchannel %d", ch);
        }

        uint32_t afc = _hunter.apFoundCount();
        if (afc > _lastApFoundCount) {
            _lastApFoundCount = afc;
            const char* ssid = _hunter.lastFoundSsid();
            _qPushOut("detected %.32s", (ssid && ssid[0]) ? ssid : "<hidden>");
        }

        uint32_t eec = _hunter.eapolEventCount();
        if (eec > _lastEapolEventCount) {
            _lastEapolEventCount = eec;
            int msg = _hunter.lastEapolMsg();
            const char* esid = _hunter.lastEapolSsid();
            _qPushOut("traced eapol M%d %.32s", msg, (esid && esid[0]) ? esid : "??");
        }

        uint32_t caps = _hunter.captureCount();
        if (caps > _lastCaptureCount) {
            _lastCaptureCount = caps;
            const char* path  = _hunter.lastCapturePath();
            const char* fname = strrchr(path, '/');
            fname = fname ? fname + 1 : path;
            _stats.onCapture();
            _stats.save();
            uint16_t r1 = 0x1000 + (uint16_t)(rand() & 0xCFFF);
            uint16_t r2 = r1 + (uint16_t)(rand() & 0x0FFF) + 0x100;
            _qPushCmd("dump 0x%04x..0x%04x >> %.27s", r1, r2, fname);
        }
        return;
    }

    _guard.update(ms);

    uint32_t dc = _guard.deauthCount();
    if (dc > _lastGuardDeauthCount) {
        _lastGuardDeauthCount = dc;
        const char* sid = _guard.lastDeauthSsid();
        _qPushOut("alert deauth %.32s", (sid && sid[0]) ? sid : "??");
        onDeauthDiscover();
    }

    uint32_t fc = _guard.beaconFloodCount();
    if (fc != _lastBeaconFloodCount) {
        _lastBeaconFloodCount = fc;
        if (fc > 0) {
            const char* sid = _guard.lastFloodSsid();
            _qPushOut("alert flood %.32s", (sid && sid[0]) ? sid : "??");
            onFloodDiscover();
        }
    }

    uint32_t tc = _guard.evilTwinCount();
    if (tc != _lastEvilTwinCount) {
        _lastEvilTwinCount = tc;
        if (tc > 0) {
            const char* sid = _guard.lastEvilTwinSsid();
            _qPushOut("alert twin %.32s", (sid && sid[0]) ? sid : "??");
            onEvilDiscover();
        }
    }
}

// ── Touch handling ────────────────────────────────────────────

void App::_handleTouch(uint32_t ms) {
    auto t = M5.Touch.getDetail();

    if (t.isPressed() || t.wasPressed() || t.wasReleased()) {
        _lastTouchMs = ms;
        if (_displayOff) {
            M5.Display.setBrightness(Theme::brightness());
            _displayOff = false;
            return;
        }
    }

    if (_menuState == MenuState::PowerWait) return;

    bool pressed  = t.isPressed();
    bool released = t.wasReleased();
    int  tx = t.x, ty = t.y;

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
        nItems = (_nethuntRunning || _nettrapRunning || _netguardRunning || s_crack.isRunning()) ? 1 : ROOT_N;
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
    int  slotCount     = paginated ? maxVis     : nItems;
    int  itemsPerPage  = paginated ? maxVis - 2 : nItems;
    int  firstItemSlot = paginated ? 1          : 0;
    int  lastItemSlot  = paginated ? maxVis - 2 : slotCount - 1;
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
        _menuHighlight = (int8_t)hitSlot;
        return;
    }

    if (!released) return;

    _menuHighlight = -1;

    if (!inMenu || hitSlot < 0) {
        menuClose();
        return;
    }

    int sel = hitSlot;

    if (paginated && sel == 0) {
        _menuScroll -= itemsPerPage;
        if (_menuScroll < 0) _menuScroll = 0;
        return;
    }
    if (paginated && sel == slotCount - 1) {
        _menuScroll += itemsPerPage;
        int maxScroll = nItems - itemsPerPage;
        if (_menuScroll > maxScroll) _menuScroll = maxScroll;
        return;
    }

    int itemIdx = _menuScroll + (sel - firstItemSlot);
    if (itemIdx < 0 || itemIdx >= nItems) {
        (void)lastItemSlot;
        menuClose();
        return;
    }

    if (_menuState == MenuState::Root) {
        if (_nethuntRunning)         { _stopNethunt();              return; }
        if (_nettrapRunning)         { _stopNettrap();              return; }
        if (_netguardRunning)        { _stopNetguard();             return; }
        if (s_crack.isRunning())     { s_crack.stop(); menuClose(); return; }
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
    s_crack.update(*this, ms);
    _updateHunting(ms);
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
