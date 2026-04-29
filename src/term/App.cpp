#include "App.h"
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

#if defined(ARDUINO_M5STACK_CORES3)
    static constexpr int SD_CS = 4;
#elif defined(ARDUINO_M5STACK_CARDPUTER)
    static constexpr int SD_CS = 12;
#else
    static constexpr int SD_CS = 4;
#endif

static NethuntCommand    s_nethunt;
static NettrapCommand    s_nettrap;
static NetguardCommand   s_netguard;
static ProfileCommand    s_profile;
static CrackCommand      s_crack;
static VaultCommand      s_vault;
static SetCommand        s_set;
static PowerOffCommand   s_poweroff;
static MenuCommand*      s_rootItems[] = {
    &s_nethunt, &s_nettrap, &s_netguard, &s_profile, &s_crack, &s_vault, &s_set, &s_poweroff
};
static constexpr int ROOT_N = (int)(sizeof(s_rootItems) / sizeof(s_rootItems[0]));

namespace {
    // ── Layout ───────────────────────────────────────────────
    constexpr int SCR_W   = 320;
    constexpr int SCR_H   = 240;

    constexpr int MARGIN   = 4;              // outer screen padding
    constexpr int BAR_H    = 15;             // bar height
    constexpr int BAR_GAP  = 2;              // gap between bars (horiz + vert)

    constexpr int HEAD_TOP_PAD = 4;
    constexpr int HEAD_BOT_PAD = 2;
    constexpr int BAR1_Y       = HEAD_TOP_PAD;                            // 4
    constexpr int BAR2_Y       = BAR1_Y + BAR_H + BAR_GAP;               // 25
    constexpr int HEAD_H       = BAR2_Y + BAR_H + HEAD_BOT_PAD;          // 46

    // Cell strip ends before the virus icon (Virus::X0 is the authoritative position)
    constexpr int CELL_RIGHT   = Virus::X0 - MARGIN;

    // ── Vertical layout: header → log → input ────────────────
    constexpr int HEADER_DIVIDER_Y = HEAD_H;                  // 38
    constexpr int LOG_TOP          = HEADER_DIVIDER_Y + 1 + 4; // 43 (4px gap)
    constexpr int INPUT_DIVIDER_Y  = SCR_H - 22;              // 218
    constexpr int LOG_BOT          = INPUT_DIVIDER_Y - 4;     // 214 (4px gap)
    constexpr int INPUT_Y          = INPUT_DIVIDER_Y + 1 + 5; // 224 (5px gap)

    constexpr int LINE_H = 9;
    constexpr int CHAR_W = 6;
    constexpr int CHAR_H = 8;

    // Max printable chars per log line, derived from screen geometry:
    //   (320 - 2×4 margin) / 6px per char = 52 chars total
    //   minus 2-char prefix ("$ " or "  ") = 50 chars of usable body
    constexpr int LOG_MAXW = (SCR_W - 2 * MARGIN) / CHAR_W; // 52
    constexpr int LOG_BODY = LOG_MAXW - 2;                   // 50

    // ── Typing animation ─────────────────────────────────────
    constexpr uint32_t TYPE_STEP_MS = 35;
    constexpr uint32_t TYPE_HOLD_MS = 400;
    constexpr uint32_t CURSOR_MS    = 480;

    // ── Menu layout ───────────────────────────────────────────
    constexpr int MENU_ITEM_H = 18;
    constexpr int BRIGHT_BH   = 14;
}

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
bool     App::typingIdle()       const { return _qCount == 0 && _typeLen == 0; }
uint32_t App::statsXp()         const { return _stats.xp(); }
uint32_t App::statsCaptures()   const { return _stats.captures(); }
uint32_t App::statsCracked()    const { return _stats.cracked(); }
uint32_t App::statsLevel()      const { return _stats.level(); }
uint32_t App::statsXpProgress() const { return _stats.xpProgress(); }
int      App::statsBattery()    const { return _stats.battery(); }
bool     App::statsCharging()   const { return _stats.isCharging(); }
void     App::onCracked()             { _stats.onCrack(); _stats.save(); }
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
    M5.Display.setBrightness(Theme::brightness());  // default until load() overrides

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
    _hunter.pause();   // starts paused; user must tap netgotchi in menu to run

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
    // Cursor blink
    if (ms - _cursorMs >= CURSOR_MS) {
        _cursorMs = ms;
        _cursorOn = !_cursorOn;
    }

    // Idle: drain any queued OUTPUT lines straight into the log
    // (no animation), then start typing the next COMMAND if there is one.
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
            // KIND_CMD — start typing animation
            _typeStepMs = ms;
            break;
        }
        return;
    }

    // Currently typing a command
    if (!_typeDone) {
        if (ms - _typeStepMs >= TYPE_STEP_MS) {
            _typeStepMs = ms;
            _typeIdx++;
            if (_typeIdx >= _typeLen) {
                _typeIdx   = _typeLen;
                _typeDone  = true;
                _typeDoneMs = ms;
            }
        }
        return;
    }

    // Done typing — hold a beat, then commit to log with "$ " prefix
    if (ms - _typeDoneMs >= TYPE_HOLD_MS) {
        char buf[LINE_COL + 4];
        snprintf(buf, sizeof(buf), "$ %s", _typeLine);
        _logPush(buf);
        _typeLine[0] = '\0';
        _typeLen = _typeIdx = 0;
        _typeDone = false;
        // Apply any pending configuration changes now that cmd is in the log
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
                      modeStr,
                      ch,
                      M5.Power.getBatteryLevel(),
                      (unsigned long)_stats.captures(),
                      (unsigned long)_stats.xp());
    }

    if (_menuState != MenuState::Closed) return;

    // ── Nethunt path ──────────────────────────────────────────────
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

    // ── Nettrap path (passive — no deauth) ───────────────────────
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

    // ── Netguard path ─────────────────────────────────────────────
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

    // Wake display on any touch
    if (t.isPressed() || t.wasPressed() || t.wasReleased()) {
        _lastTouchMs = ms;
        if (_displayOff) {
            M5.Display.setBrightness(Theme::brightness());
            _displayOff = false;
            return;  // consume the touch as a wake-only event
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

    // Reset scroll when the underlying list changes (e.g. CrackCommand pcap → dict).
    if (_menuLastSubCount != nItems) {
        _menuScroll       = 0;
        _menuLastSubCount = nItems;
    }

    int maxVis = (INPUT_DIVIDER_Y - HEADER_DIVIDER_Y - 4) / itemH;
    if (maxVis < 3)        maxVis = 3;
    bool paginated         = nItems > maxVis;
    int  slotCount         = paginated ? maxVis      : nItems;
    int  itemsPerPage      = paginated ? maxVis - 2  : nItems;
    int  firstItemSlot     = paginated ? 1           : 0;
    int  lastItemSlot      = paginated ? maxVis - 2  : slotCount - 1;
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

    // Pagination controls — don't dispatch to the menu item.
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
        if (_nethuntRunning)                        { _stopNethunt();   return; }
        if (_nettrapRunning)                        { _stopNettrap();   return; }
        if (_netguardRunning)                       { _stopNetguard();  return; }
        if (s_crack.isRunning())     { s_crack.stop(); menuClose(); return; }
        s_rootItems[itemIdx]->execute(*this);
        return;
    }

    if (_menuState == MenuState::Sub && _activeSubCmd) {
        _activeSubCmd->onSubSelect(*this, itemIdx);
        return;
    }
}

// ── Header ────────────────────────────────────────────────────

// Draw a bar with [LABEL ==== VALUE] rendered inside.
//   - unfilled portion: pale green bg
//   - filled portion: bright green fill
//   - label/value text: black, sits on top of either bg
static void drawValueBar(M5Canvas& c,
                         int barX, int barY, int barW, int barH,
                         const char* label, const char* valText, int fillPct)
{
    if (barW < 8 || barH < 6) return;
    if (fillPct < 0)   fillPct = 0;
    if (fillPct > 100) fillPct = 100;

    // Pale-green background (unfilled), with dim-green border
    c.fillRect(barX, barY, barW, barH, Theme::PALE);
    c.drawRect(barX, barY, barW, barH, Theme::DIM);

    const int innerW = barW - 2;
    const int innerH = barH - 2;
    const int innerX = barX + 1;
    const int innerY = barY + 1;
    const int fill   = innerW * fillPct / 100;
    if (fill > 0) c.fillRect(innerX, innerY, fill, innerH, Theme::FG);

    // Black text overlays both fills
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Theme::BG);
    const int midY = barY + barH / 2 + 1;
    const int padL = (barH - CHAR_H) / 2;   // left/right pad = vertical gap to bar edge

    c.setTextDatum(lgfx::middle_left);
    c.drawString(label, barX + padL + 1, midY);

    c.setTextDatum(lgfx::middle_right);
    c.drawString(valText, barX + barW - padL + 1, midY);
}

void App::_drawHud(M5Canvas& c, uint32_t ms) const {
    c.fillRect(0, 0, SCR_W, HEAD_H, Theme::BG);

    // Battery
    int  bat      = _stats.battery();
    bool charging = _stats.isCharging();

    // RAM (used %)
    uint32_t freeH  = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
                    + (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t totalH = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_INTERNAL)
                    + (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    int ram = (totalH > 0) ? (int)((uint64_t)(totalH - freeH) * 100u / totalH) : 0;
    if (ram > 100) ram = 100;
    if (ram < 0)   ram = 0;

    // ── Row 1: [BAT === xx%]  [RAM === xx%] ──────────────────
    const int stripW = CELL_RIGHT - MARGIN;
    const int eachW  = (stripW - BAR_GAP) / 2;
    const int batX   = MARGIN;
    const int ramX   = batX + eachW + BAR_GAP;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", bat);
    drawValueBar(c, batX, BAR1_Y, eachW, BAR_H, charging ? "BAT++" : "BAT", buf, bat);

    snprintf(buf, sizeof(buf), "%d%%", ram);
    drawValueBar(c, ramX, BAR1_Y, eachW, BAR_H, "RAM", buf, ram);

    // ── Row 2: [EXP ====================== LVx] ───────────────
    snprintf(buf, sizeof(buf), "LV%lu", (unsigned long)_stats.level());
    drawValueBar(c, MARGIN, BAR2_Y, stripW, BAR_H, "EXP", buf, (int)_stats.xpProgress());

    // ── Header bottom divider ────────────────────────────────
    c.drawFastHLine(0, HEADER_DIVIDER_Y, SCR_W, Theme::DIM);

    Virus::State vs;
    if      (s_crack.isRunning())       vs = Virus::State::Decrypting;
    else if (_nethuntRunning && _exhaustPhase != 0) vs = Virus::State::Sleep;
    else if (_nethuntRunning)                       vs = Virus::State::Active;
    else if (_nettrapRunning)                       vs = Virus::State::Trap;
    else if (_netguardRunning)                      vs = Virus::State::Guard;
    else                                            vs = Virus::State::Idle;
    Virus::draw(c, ms, vs);
}

// ── Scrollback ────────────────────────────────────────────────

void App::_drawLog(M5Canvas& c) const {
    c.fillRect(0, LOG_TOP, SCR_W, LOG_BOT - LOG_TOP, Theme::BG);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Theme::FG, Theme::BG);
    c.setTextDatum(lgfx::top_left);

    const int maxVis      = (LOG_BOT - LOG_TOP) / LINE_H;
    const bool cracking   = (s_crack.isRunning());
    int        scrollSlot = 0;

    // While cracking, the bottom-most line is a live progress bar that
    // updates in place; existing log entries scroll up one slot.
    if (cracking) {
        uint32_t pct = (s_crack.fileSize() > 0)
            ? (uint32_t)((uint64_t)s_crack.bytesDone() * 100 / s_crack.fileSize())
            : 0;
        if (pct > 100) pct = 100;
        char bar[21]; int filled = (int)(20 * pct / 100);
        for (int i = 0; i < 20; i++) bar[i] = (i < filled) ? '#' : ' ';
        bar[20] = '\0';

        char speedBuf[10] = "";
        char etaBuf[10]   = "";
        uint32_t elapsed_s = (millis() - s_crack.startMs() + 500) / 1000;
        if (elapsed_s > 0) {
            uint32_t wps = s_crack.tested() / elapsed_s;
            if (wps >= 1000) snprintf(speedBuf, sizeof(speedBuf), " %luk/s", (unsigned long)(wps / 1000));
            else             snprintf(speedBuf, sizeof(speedBuf), " %lu/s",  (unsigned long)wps);

            uint32_t bps = (s_crack.bytesDone() > 0) ? s_crack.bytesDone() / elapsed_s : 0;
            if (bps > 0 && s_crack.fileSize() > s_crack.bytesDone()) {
                uint32_t eta = (s_crack.fileSize() - s_crack.bytesDone()) / bps;
                if (eta < 60)   snprintf(etaBuf, sizeof(etaBuf), " %lus",       (unsigned long)eta);
                else            snprintf(etaBuf, sizeof(etaBuf), " %lum%02lus",  (unsigned long)(eta / 60), (unsigned long)(eta % 60));
            }
        }

        char buf[LINE_COL];
        snprintf(buf, sizeof(buf), "[%s] %lu%%%s%s", bar, (unsigned long)pct, speedBuf, etaBuf);
        int y = LOG_BOT - LINE_H + 1;
        c.drawString(buf, MARGIN, y);
        scrollSlot = 1;  // shift normal log lines up by one slot
    }

    // Newest log line just above the progress bar (or at the bottom if idle)
    for (int i = 0; i + scrollSlot < maxVis && i < LOG_LINES; i++) {
        int idx = (_logHead - 1 - i + LOG_LINES * 4) % LOG_LINES;
        if (_logBuf[idx][0] == '\0') break;
        int y = LOG_BOT - LINE_H * (i + 1 + scrollSlot) + 1;
        c.drawString(_logBuf[idx], MARGIN, y);
    }
}

// ── Input prompt ──────────────────────────────────────────────

void App::_drawInput(M5Canvas& c) const {
    c.drawFastHLine(0, INPUT_DIVIDER_Y, SCR_W, Theme::DIM);
    c.fillRect(0, INPUT_DIVIDER_Y + 1, SCR_W, SCR_H - (INPUT_DIVIDER_Y + 1), Theme::BG);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Theme::FG, Theme::BG);
    c.setTextDatum(lgfx::top_left);

    const int promptX = MARGIN;
    c.drawString("$ ", promptX, INPUT_Y);

    // Typed-so-far slice
    char buf[LINE_COL];
    int n = _typeIdx;
    if (n > LINE_COL - 1) n = LINE_COL - 1;
    memcpy(buf, _typeLine, n);
    buf[n] = '\0';
    const int textX = promptX + 2 * CHAR_W;
    c.drawString(buf, textX, INPUT_Y);

    // Cursor block — solid while typing, blink when idle/done
    bool show = true;
    if (_typeLen == 0 || _typeDone) show = _cursorOn;
    if (show) {
        int cx = textX + n * CHAR_W;
        c.fillRect(cx, INPUT_Y, CHAR_W - 1, CHAR_H, Theme::FG);
    }
}

// ── Menu overlay (floats above input, log visible above) ─────────

void App::_drawMenuContent(M5Canvas& c) const {
    c.setFont(&fonts::Font0);
    c.setTextSize(1);

    int nItems = 0, itemH = MENU_ITEM_H;
    if (_menuState == MenuState::Root) {
        nItems = (_nethuntRunning || _nettrapRunning || _netguardRunning || s_crack.isRunning()) ? 1 : ROOT_N;
    } else if (_menuState == MenuState::Sub && _activeSubCmd) {
        nItems = _activeSubCmd->subCount();
        itemH  = _activeSubCmd->subItemH();
    }

    int maxVis = (INPUT_DIVIDER_Y - HEADER_DIVIDER_Y - 4) / itemH;
    if (maxVis < 3) maxVis = 3;
    bool paginated     = nItems > maxVis;
    int  slotCount     = paginated ? maxVis     : nItems;
    int  itemsPerPage  = paginated ? maxVis - 2 : nItems;
    int  firstItemSlot = paginated ? 1          : 0;
    int  scroll        = _menuScroll;
    if (paginated) {
        int maxScroll = nItems - itemsPerPage;
        if (scroll < 0)         scroll = 0;
        if (scroll > maxScroll) scroll = maxScroll;
    } else {
        scroll = 0;
    }
    int menuTop = INPUT_DIVIDER_Y - slotCount * itemH;

    c.fillRect(0, menuTop, SCR_W, INPUT_DIVIDER_Y - menuTop, Theme::BG);
    c.drawFastHLine(0, menuTop, SCR_W, Theme::DIM);

    auto drawItem = [&](int slot, const char* label, bool active) {
        int y   = menuTop + slot * itemH;
        bool hi = ((int)_menuHighlight == slot);
        uint16_t col = active ? Theme::FG : Theme::DIM;
        if (hi) c.fillRect(0, y, SCR_W, itemH, Theme::PALE);
        c.setTextColor(col, hi ? Theme::PALE : Theme::BG);
        c.setTextDatum(lgfx::middle_left);
        c.drawString(label, MARGIN + 6, y + itemH / 2);
    };

    auto drawNav = [&](int slot, const char* label, bool enabled) {
        int y   = menuTop + slot * itemH;
        bool hi = ((int)_menuHighlight == slot);
        if (hi) c.fillRect(0, y, SCR_W, itemH, Theme::PALE);
        c.setTextColor(enabled ? Theme::FG : Theme::DIM, hi ? Theme::PALE : Theme::BG);
        c.setTextDatum(lgfx::middle_center);
        c.drawString(label, SCR_W / 2, y + itemH / 2);
    };

    for (int slot = 0; slot < slotCount; slot++) {
        if (paginated && slot == 0) {
            drawNav(slot, "<< prev", scroll > 0);
            continue;
        }
        if (paginated && slot == slotCount - 1) {
            bool more = (scroll + itemsPerPage) < nItems;
            drawNav(slot, "next >>", more);
            continue;
        }

        int itemIdx = scroll + (slot - firstItemSlot);
        if (itemIdx < 0 || itemIdx >= nItems) continue;

        if (_menuState == MenuState::Root) {
            const bool locked = _nethuntRunning || _nettrapRunning || _netguardRunning || (s_crack.isRunning());
            const char* lbl = locked ? "stop" : s_rootItems[itemIdx]->label();
            drawItem(slot, lbl, true);
        } else if (_menuState == MenuState::Sub && _activeSubCmd) {
            bool act = _activeSubCmd->subIsActive(itemIdx);
            bool lit = !_activeSubCmd->subUseDim() || act;
            drawItem(slot, _activeSubCmd->subLabel(itemIdx), lit);
            if (act) {
                int y = menuTop + slot * itemH;
                c.setTextDatum(lgfx::middle_right);
                c.drawString("*", SCR_W - MARGIN - 6, y + itemH / 2);
            }
        }
    }

    // Input area — show parent command label as typed hint while sub-menu is open
    c.drawFastHLine(0, INPUT_DIVIDER_Y, SCR_W, Theme::DIM);
    c.fillRect(0, INPUT_DIVIDER_Y + 1, SCR_W, SCR_H - (INPUT_DIVIDER_Y + 1), Theme::BG);
    c.setTextColor(Theme::FG, Theme::BG);
    c.setTextDatum(lgfx::top_left);
    c.drawString("$ ", MARGIN, INPUT_Y);

    const char* inputText = (_menuState == MenuState::Sub && _activeSubCmd)
                            ? _activeSubCmd->inputHint() : "";
    const int textX = MARGIN + 2 * CHAR_W;
    if (inputText[0]) c.drawString(inputText, textX, INPUT_Y);
    if (_cursorOn) {
        int cx = textX + (int)strlen(inputText) * CHAR_W;
        c.fillRect(cx, INPUT_Y, CHAR_W - 1, CHAR_H, Theme::FG);
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

    // Display-off timeout
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
