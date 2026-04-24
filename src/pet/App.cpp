#include "App.h"
#include "../core/Palette.h"
#include "../core/RandomSeed.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>

// ── Capture celebration pool ──────────────────────────────────

static const char* CAPTURE_MSGS[] = {
    "pcap saved!",
    "handshake!",
    "crack time!",
    "got the key",
};
static constexpr int CAPTURE_N = sizeof(CAPTURE_MSGS) / sizeof(CAPTURE_MSGS[0]);

// ── Found-WiFi dialog ─────────────────────────────────────────

static const char* FOUND_MSGS[] = {
    "new target!",
    "wifi spotted",
    "locked on!",
    "ooh shiny~",
    "mine now...",
    "got one!!",
    "target acq!",
};
static constexpr int FOUND_N = sizeof(FOUND_MSGS) / sizeof(FOUND_MSGS[0]);

// ── Pre-hack taunts (said before attack starts) ───────────────

static const char* HACK_TAUNTS[] = {
    "time 2 hack~",
    "lets pwn dis",
    "hack mode on",
    "pwning soon~",
    "init attack!",
    "gonna crack!",
    "let me in...",
    "no mercy >:]",
};
static constexpr int HACK_TAUNT_N = sizeof(HACK_TAUNTS) / sizeof(HACK_TAUNTS[0]);

// ── Zero-result idle taunts ───────────────────────────────────

static const char* IDLE_MSGS[] = {
    "no wifi here",
    "just static",
    "dead zone...",
    "ghost town",
    "null packets",
    "404 wifi :(",
    "sniffer sad",
    "nothing...",
    "signal? nope",
    "so empty rn",
};
static constexpr int IDLE_N = sizeof(IDLE_MSGS) / sizeof(IDLE_MSGS[0]);

// ── SD card CS pin ────────────────────────────────────────────

#if defined(ARDUINO_M5STACK_CORES3)
    static constexpr int SD_CS = 4;
#elif defined(ARDUINO_M5STACK_CARDPUTER)
    static constexpr int SD_CS = 12;
#else
    static constexpr int SD_CS = 4;
#endif

// ── Init ──────────────────────────────────────────────────────

void App::init() {
    Serial.begin(115200);
    RandomSeed::init();
    _zeroSweepThresh = (uint8_t)random(2, 7);

    if (psramFound()) {
        psramInit();
        Serial.printf("[INIT] PSRAM %uKB free\n", ESP.getFreePsram() / 1024);
    } else {
        Serial.println("[INIT] No PSRAM");
    }

    auto cfg = M5.config();
    M5.begin(cfg);

    int physW = M5.Display.width();
    int physH = M5.Display.height();
    SCALE     = physW / SCREEN_W;
    SCREEN_H  = physH / SCALE;
    BTN_STRIP = SCREEN_H * 11 / 60;
    GROUND_Y  = SCREEN_H * 54 / 60;

    Serial.printf("[INIT] display=%dx%d scale=%d virtual=%dx%d hud=%d ground=%d\n",
                  physW, physH, SCALE, SCREEN_W, SCREEN_H, BTN_STRIP, GROUND_Y);

    _canvas = new M5Canvas(&M5.Display);
    _canvas->setColorDepth(16);
    _canvas->createSprite(physW, physH);

    _pixelCanvas = new M5Canvas(&M5.Display);
    _pixelCanvas->setColorDepth(16);
    _pixelCanvas->createSprite(SCREEN_W, SCREEN_H);

    // ── SD card ───────────────────────────────────────────────
    if (!SD.begin(SD_CS)) {
        Serial.println("[SD] Mount failed — captures won't be saved");
    } else {
        Serial.printf("[SD] OK  %lluMB total\n", SD.totalBytes() / (1024 * 1024));
        SD.mkdir("/netgotchi");
        SD.mkdir("/netgotchi/eapol");
    }

    _stats.load();
    _hunter.init();

    uint32_t ms = millis();
    _applyHuntState(HuntState::Listing, ms);
    Serial.println("[INIT] Boot complete — hunting started");
}

// ── State transitions ─────────────────────────────────────────

static const char* const STATE_NAMES[] = { "Idle","Listing","Deauthing","Waiting","Captured","Taunting" };

void App::_applyHuntState(HuntState next, uint32_t ms) {
    if (_huntState != next)
        Serial.printf("[STATE] %s -> %s\n",
                      STATE_NAMES[static_cast<int>(_huntState)],
                      STATE_NAMES[static_cast<int>(next)]);

    _huntState    = next;
    _stateEntryMs = ms;
    _quipMs       = 0;

    switch (next) {
        case HuntState::Listing:
            _pet.setStayRight(false);
            _pet.setAnim(Anim::Idle);
            _pet.setSpeech(nullptr);
            break;

        case HuntState::Deauthing: {
            _pet.setStayRight(true);
            _pet.setAnim(Anim::Walk);
            _pet.setSpeech(nullptr);
            char dBuf[14];
            snprintf(dBuf, sizeof(dBuf), "> deauth ch%d", _hunter.channel());
            _termPush(dBuf);
            break;
        }
        case HuntState::Waiting: {
            _pet.setStayRight(true);
            _pet.setAnim(Anim::Walk);
            _pet.setSpeech(nullptr);
            char wBuf[14];
            snprintf(wBuf, sizeof(wBuf), "> eapol ch%d", _hunter.channel());
            _termPush(wBuf);
            break;
        }
        case HuntState::Captured:
            _pet.setStayRight(false);
            _pet.setAnim(Anim::Talk);
            _pet.setSpeech(CAPTURE_MSGS[0]);
            _termPush("> handshake!");
            _termPush("> pcap saved");
            break;

        case HuntState::Taunting: {
            _pet.setStayRight(false);
            _pet.setAnim(Anim::Talk);
            const char* taunt = HACK_TAUNTS[random(HACK_TAUNT_N)];
            _pet.setSpeech(taunt);
            _termPush(taunt);
            break;
        }

        case HuntState::Idle:
        default:
            _pet.setStayRight(false);
            _pet.setAnim(Anim::Idle);
            _zeroSweepCount++;
            if (_zeroSweepCount >= _zeroSweepThresh) {
                _zeroSweepCount  = 0;
                _zeroSweepThresh = (uint8_t)random(2, 7);
                const char* taunt = IDLE_MSGS[random(IDLE_N)];
                _pet.setSpeech(taunt);
                _termPush(taunt);
            } else {
                _pet.setSpeech(nullptr);
                _termPush("> no targets");
            }
            break;
    }
}

// ── Capture event ─────────────────────────────────────────────

void App::_onCapture(uint32_t ms) {
    bool leveled = _stats.onCapture();

    _applyHuntState(HuntState::Captured, ms);

    if (leveled) {
        Serial.printf("[LEVELUP] Now level %d!\n", _stats.level());
        snprintf(_speechBuf, sizeof(_speechBuf), "lvl %d! owo", _stats.level());
        _pet.setSpeech(_speechBuf);
        _termPush(_speechBuf);
        _stats.save();
        return;
    }
    if (_stats.achMsg()) {
        _pet.setSpeech(_stats.achMsg());
        _termPush(_stats.achMsg());
    }
    _stats.save();
}

// ── Scrolling terminal ring buffer ────────────────────────────

void App::_termPush(const char* line) {
    strncpy(_termBuf[_termHead], line, sizeof(_termBuf[0]) - 1);
    _termBuf[_termHead][sizeof(_termBuf[0]) - 1] = '\0';
    _termHead = (_termHead + 1) % TERM_LINES;
}

// ── Per-frame hunt logic ──────────────────────────────────────

void App::_updateHunting(uint32_t ms) {
    // Periodic status log every 10 s
    if (ms - _lastStatusLogMs >= 10000) {
        _lastStatusLogMs = ms;
        Serial.printf("[STATUS] state=%s ch=%d bat=%d%% mem=%uKB caps=%lu lvl=%d\n",
                      STATE_NAMES[static_cast<int>(_huntState)],
                      _hunter.channel(),
                      M5.Power.getBatteryLevel(),
                      (unsigned)((ESP.getFreeHeap() + ESP.getFreePsram()) / 1024),
                      (unsigned long)_stats.totalCaptures(),
                      _stats.level());
    }

    _hunter.update(ms);

    // Log channel hops during scan
    if (_huntState == HuntState::Listing) {
        uint8_t ch = (uint8_t)_hunter.channel();
        if (ch != _lastTermCh) {
            _lastTermCh = ch;
            char buf[14];
            snprintf(buf, sizeof(buf), "> scan ch%d", ch);
            _termPush(buf);
        }
    }

    // Detect newly found APs — show found-wifi dialog during scan
    uint8_t nowAps = _hunter.apCount();
    if (nowAps > _lastApCount) {
        if (_huntState == HuntState::Listing) {
            const char* ssid = _hunter.lastSsid();
            if (ssid[0] != '\0') {
                snprintf(_speechBuf, sizeof(_speechBuf), "found %.9s", ssid);
                _pet.setSpeech(_speechBuf);
                char tBuf[14];
                snprintf(tBuf, sizeof(tBuf), "> %.11s", ssid);
                _termPush(tBuf);
            } else {
                _pet.setSpeech(FOUND_MSGS[random(FOUND_N)]);
                _termPush("> hidden wifi");
            }
            _termPush(FOUND_MSGS[random(FOUND_N)]);
        }
        _lastApCount = nowAps;
    }

    // Check for new EAPOL captures
    uint32_t caps = _hunter.captureCount();
    if (caps > _lastCaptureCount) {
        _lastCaptureCount = caps;
        _onCapture(ms);
        return;
    }

    // Stay in Captured state for 6 s after trigger
    if (_huntState == HuntState::Captured) {
        if (ms - _stateEntryMs < 6000) {
            if (ms - _quipMs >= 2000) {
                _quipMs = ms;
                _pet.setSpeech(CAPTURE_MSGS[(ms / 2000) % CAPTURE_N]);
            }
            return;
        }
    }

    // Hold in Taunting state for 3.5 s before attacking
    if (_huntState == HuntState::Taunting) {
        if (_hunter.phase() != WiFiHunter::Phase::Attacking) {
            _applyHuntState(HuntState::Listing, ms);
        } else if (ms - _stateEntryMs >= 3500) {
            _applyHuntState(
                _hunter.isDeauthing() ? HuntState::Deauthing : HuntState::Waiting, ms);
        }
        return;
    }

    // Sync animation to WiFiHunter phase
    HuntState desired;
    if (_hunter.phase() == WiFiHunter::Phase::Attacking) {
        if (_huntState == HuntState::Listing || _huntState == HuntState::Idle) {
            desired = HuntState::Taunting;  // say taunt before attacking
        } else {
            desired = _hunter.isDeauthing() ? HuntState::Deauthing : HuntState::Waiting;
        }
    } else if (_hunter.isScanCooldown()) {
        desired = HuntState::Idle;
    } else {
        desired = HuntState::Listing;
    }
    if (desired != _huntState)
        _applyHuntState(desired, ms);
}

// ── HUD — drawn on physical canvas, never scaled ─────────────

void App::_drawHud(M5Canvas& c) const {
    const int physW      = c.width();
    const int physH      = c.height();
    const int GAP        = 2;
    const int barH       = 13;
    const int leftX      = 2;
    const int barY       = 2;
    const int barW       = (physW - leftX * 2 - GAP) / 2;
    const int rightX     = leftX + barW + GAP;
    const int midY       = barY + barH / 2;

    int hp = M5.Power.getBatteryLevel();
    if (hp < 0) hp = 50;
    if (hp > 100) hp = 100;

    // Use heap_caps directly — ESP.getPsramSize() can return 0 on some SDK builds
    uint32_t freeH  = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
                    + (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t totalH = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_INTERNAL)
                    + (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    int brain = (totalH > 0) ? (int)((uint64_t)(totalH - freeH) * 100u / totalH) : 50;
    if (brain > 100) brain = 100;
    if (brain < 0)   brain = 0;

    const int expY = barY + barH + 2;
    const int expH = barH;
    const int expW = physW - leftX * 2;
    const int bgH  = expY + expH + barY;
    c.fillRect(0, 0, physW, bgH, Palette::HudBg);

    // HP bar (left) — dark green
    c.fillRect(leftX, barY, barW, barH, Palette::BarBg);
    c.drawRect(leftX, barY, barW, barH, Palette::BarBdr);
    int hpFill = (barW - 2) * hp / 100;
    if (hpFill > 0) c.fillRect(leftX + 1, barY + 1, hpFill, barH - 2, Palette::HpFill);

    // Brain bar (right) — dark violet
    c.fillRect(rightX, barY, barW, barH, Palette::BarBg);
    c.drawRect(rightX, barY, barW, barH, Palette::BarBdr);
    int brFill = (barW - 2) * brain / 100;
    if (brFill > 0) c.fillRect(rightX + 1, barY + 1, brFill, barH - 2, Palette::BrainFill);

    // Labels
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Palette::White);

    c.setTextDatum(lgfx::middle_left);
    c.drawString("HP", leftX + 3, midY + 1);
    char hpBuf[5];
    snprintf(hpBuf, sizeof(hpBuf), "%d%%", hp);
    c.setTextDatum(lgfx::middle_right);
    c.drawString(hpBuf, leftX + barW - 2, midY + 1);

    c.setTextDatum(lgfx::middle_left);
    c.drawString("Brain", rightX + 3, midY + 1);
    char brBuf[5];
    snprintf(brBuf, sizeof(brBuf), "%d%%", brain);
    c.setTextDatum(lgfx::middle_right);
    c.drawString(brBuf, rightX + barW - 2, midY + 1);

    // EXP bar (full width, dark gold) — progress within current level
    uint8_t  lv       = _stats.level();
    uint32_t expFloor = (uint32_t)(lv - 1) * 50;
    uint32_t expRem   = (_stats.exp() > expFloor) ? (_stats.exp() - expFloor) : 0;
    int      expMidY  = expY + expH / 2;
    int      expFill  = (int)((uint64_t)expRem * (uint32_t)(expW - 2) / 50);
    if (expFill > expW - 2) expFill = expW - 2;

    c.fillRect(leftX, expY, expW, expH, Palette::BarBg);
    c.drawRect(leftX, expY, expW, expH, Palette::BarBdr);
    if (expFill > 0) c.fillRect(leftX + 1, expY + 1, expFill, expH - 2, Palette::ExpFill);

    char lvlBuf[8];
    snprintf(lvlBuf, sizeof(lvlBuf), "LVL %d", lv);
    c.setTextDatum(lgfx::middle_left);
    c.drawString(lvlBuf, leftX + 3, expMidY + 1);

    char expBuf[10];
    snprintf(expBuf, sizeof(expBuf), "%lu/50", (unsigned long)expRem);
    c.setTextDatum(lgfx::middle_right);
    c.drawString(expBuf, leftX + expW - 2, expMidY + 1);

    (void)physH;
}

// ── Terminal dialog — physical canvas, left side, hacking phase only

void App::_drawTerminal(M5Canvas& c) const {
    if (_huntState != HuntState::Deauthing &&
        _huntState != HuntState::Waiting   &&
        _huntState != HuntState::Captured) return;

    // Position below the HUD (barY=2, barH=13, expY=17, expH=13, bgH=32)
    const int hudBotY = 32;
    const int pad     = 2;
    const int lineH   = 8;
    const int lines   = 3;
    const int boxW    = 84;
    const int boxH    = lines * lineH + pad * 2;
    const int boxX    = 4;
    const int boxY    = hudBotY + 4;

    c.fillRect(boxX, boxY, boxW, boxH, Palette::TermBg);
    c.drawRect(boxX, boxY, boxW, boxH, Palette::TermBdr);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Palette::TermText);
    c.setTextDatum(lgfx::top_left);

    for (int i = 0; i < lines; i++) {
        int idx = (_termHead - lines + i + TERM_LINES * 4) % TERM_LINES;
        if (_termBuf[idx][0] == '\0') continue;
        c.drawString(_termBuf[idx], boxX + pad, boxY + pad + i * lineH);
    }
}

// ── Main loop ─────────────────────────────────────────────────

void App::update() {
    M5.update();
    uint32_t ms = millis();

    _pet.update(ms);
    _updateHunting(ms);

    _pixelCanvas->fillScreen(Palette::SkyBot);
    _bg.draw(*_pixelCanvas);
    _pet.draw(*_pixelCanvas);

    int physW = M5.Display.width(), physH = M5.Display.height();
    _pixelCanvas->pushRotateZoom(_canvas, physW / 2, physH / 2, 0.0f,
                                 (float)SCALE, (float)SCALE);
    _drawHud(*_canvas);
    _drawTerminal(*_canvas);
    _canvas->pushSprite(0, 0);
}
