#include "App.h"
#include "../core/Palette.h"
#include "../core/RandomSeed.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>

// ── Pre-attack taunts ─────────────────────────────────────────

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

    if (psramFound()) {
        psramInit();
        Serial.printf("[INIT] PSRAM %uKB free\n", ESP.getFreePsram() / 1024);
    } else {
        Serial.println("[INIT] No PSRAM");
    }

    auto cfg = M5.config();
    M5.begin(cfg);

    _canvas = new M5Canvas(&M5.Display);
    _canvas->setColorDepth(16);
    _canvas->createSprite(SCREEN_W, SCREEN_H);

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
    _applyHuntState(HuntState::Idle, ms);
    Serial.println("[INIT] Boot complete — hunting started");
}

// ── State transitions ─────────────────────────────────────────

static const char* const STATE_NAMES[] = { "Idle","PreAttack","Attacking","Captured","Done","Menu" };

void App::_applyHuntState(HuntState next, uint32_t ms) {
    if (_huntState != next)
        Serial.printf("[STATE] %s -> %s\n",
                      STATE_NAMES[static_cast<int>(_huntState)],
                      STATE_NAMES[static_cast<int>(next)]);

    _huntState    = next;
    _stateEntryMs = ms;
    _quipMs       = 0;

    switch (next) {
        case HuntState::Idle:
            _pet.setStayRight(false);
            _pet.setPinCenter(false);
            _pet.setAnim(Anim::Walk);
            _pet.setSpeech(nullptr);
            break;

        case HuntState::Menu:
            _pet.setPinCenter(false);
            _pet.setStayRight(true);
            _pet.setAnim(Anim::Walk);
            _pet.setSpeech(nullptr);
            _menuReady = false;
            break;

        case HuntState::PreAttack:
            _pet.setPinCenter(true);
            _pet.setAnim(Anim::Walk);
            _pet.setSpeech(HACK_TAUNTS[random(HACK_TAUNT_N)]);
            break;

        case HuntState::Attacking:
            _pet.setPinCenter(true);
            _pet.setAnim(Anim::Walk);
            _pet.setSpeech(nullptr);
            _lastChannel = -1;
            break;

        case HuntState::Captured: {
            _pet.setPinCenter(false);
            _pet.setAnim(Anim::Idle);
            const char* ssid = _hunter.lastSsid();
            if (ssid && ssid[0])
                snprintf(_speechBuf, sizeof(_speechBuf), "got %.17s!", ssid);
            else
                strncpy(_speechBuf, "got hidden net!", sizeof(_speechBuf) - 1);
            _pet.setSpeech(_speechBuf);
            break;
        }

        case HuntState::Done:
        default:
            _pet.setPinCenter(false);
            _pet.setAnim(Anim::Idle);
            _pet.setSpeech("done!!");
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

    // Pause hunter while menu is open
    if (_huntState != HuntState::Menu) {
        _hunter.update(ms);

        // New EAPOL capture — highest priority
        uint32_t caps = _hunter.captureCount();
        if (caps > _lastCaptureCount) {
            _lastCaptureCount = caps;
            _onCapture(ms);
            return;
        }
    }

    switch (_huntState) {
        case HuntState::Menu: {
            if (!_menuReady) {
                const int su = SCREEN_H / 10, bW = su + su / 4;
                if ((int)_pet.posX() >= SCREEN_W - bW - 3) {
                    _menuReady = true;
                    _pet.setAnim(Anim::Idle);  // hold position, blink only
                }
            }
            break;
        }

        case HuntState::Idle:
            if (_hunter.phase() == WiFiHunter::Phase::Attacking)
                _applyHuntState(HuntState::PreAttack, ms);
            break;

        case HuntState::PreAttack:
            if (_hunter.phase() != WiFiHunter::Phase::Attacking) {
                _applyHuntState(HuntState::Idle, ms);
            } else if (!_pet.speechActive()) {
                _applyHuntState(HuntState::Attacking, ms);
            }
            break;

        case HuntState::Attacking: {
            if (_hunter.phase() != WiFiHunter::Phase::Attacking) {
                _applyHuntState(HuntState::Idle, ms);
                break;
            }
            int ch = _hunter.channel();
            if (ch != _lastChannel) {
                _lastChannel = ch;
                snprintf(_speechBuf, sizeof(_speechBuf), "ch%d!", ch);
                _pet.setSpeech(_speechBuf);
            }
            break;
        }

        case HuntState::Captured:
            if (!_pet.speechActive())
                _applyHuntState(HuntState::Done, ms);
            break;

        case HuntState::Done:
            if (ms - _stateEntryMs >= 5000)
                _applyHuntState(HuntState::Idle, ms);
            break;
    }
}

// ── HUD — drawn on physical canvas, never scaled ─────────────

void App::_drawHud(M5Canvas& c) const {
    const int physW      = c.width();
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

}

// ── Terminal dialog — physical canvas, left side, hacking phase only

void App::_drawTerminal(M5Canvas& c) const {
    bool hasContent = false;
    for (int i = 0; i < TERM_LINES; i++) {
        if (_termBuf[i][0] != '\0') { hasContent = true; break; }
    }
    if (!hasContent) return;

    const int pad   = 2;
    const int lineH = 8;   // Font0 at size 1

    // Right border: 4 px gap from slime's left body edge when at right wall
    const int su      = SCREEN_H / 10;
    const int bW      = su + su / 4;
    const int rBound  = SCREEN_W - bW - 2;
    const int boxRX   = rBound - bW - 4;

    // Vertical: just below HUD, down to ground
    const int boxX    = 4;
    const int boxY    = BTN_STRIP;
    const int boxBotY = GROUND_Y;
    const int boxW    = boxRX - boxX;
    const int boxH    = boxBotY - boxY;
    const int lines   = (boxH - pad * 2) / lineH;

    if (boxW < 20 || lines < 1) return;

    c.fillRect(boxX, boxY, boxW, boxH, Palette::TermBg);
    c.drawRect(boxX, boxY, boxW, boxH, Palette::TermBdr);

    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextColor(Palette::TermText);
    c.setTextDatum(lgfx::top_left);

    int vis = (lines < TERM_LINES) ? lines : TERM_LINES;
    for (int i = 0; i < vis; i++) {
        int idx = (_termHead - vis + i + TERM_LINES * 4) % TERM_LINES;
        if (_termBuf[idx][0] == '\0') continue;
        c.drawString(_termBuf[idx], boxX + pad, boxY + pad + i * lineH);
    }
}

// ── Touch handler ─────────────────────────────────────────────

void App::_handleTouch(uint32_t ms) {
    auto touch = M5.Touch.getDetail();
    if (!touch.wasPressed()) return;
    int tx = touch.x;
    int ty = touch.y;

    if (_huntState == HuntState::Menu) {
        if (!_menuReady) return;
        const int su    = SCREEN_H / 10, bW = su + su / 4;
        const int panX  = 4, panY = BTN_STRIP;
        const int panW  = (SCREEN_W - bW - 2) - bW - 4 - panX;  // boxRX - panX
        const int panH  = GROUND_Y - BTN_STRIP;
        const int btnX  = panX + 8, btnW = panW - 16, btnH = 26;
        const int btn1Y = panY + panH / 2 + 4;
        const int btn2Y = btn1Y + btnH + 8;

        if (tx >= btnX && tx < btnX + btnW) {
            if (ty >= btn1Y && ty < btn1Y + btnH) {
                _pet.setSpeech("cracking..");
                _applyHuntState(HuntState::Idle, ms);
            } else if (ty >= btn2Y && ty < btn2Y + btnH) {
                _applyHuntState(HuntState::Idle, ms);
            }
        }
    } else {
        // Tap on slime body → open menu
        const int su = SCREEN_H / 10, bW = su + su / 4;
        int cx = (int)_pet.posX();
        int cy = GROUND_Y - bW;
        if (tx >= cx - bW && tx <= cx + bW && ty >= cy - bW && ty <= GROUND_Y)
            _applyHuntState(HuntState::Menu, ms);
    }
}

// ── Menu overlay ──────────────────────────────────────────────

void App::_drawMenu(M5Canvas& c) const {
    const int su   = SCREEN_H / 10, bW = su + su / 4;
    const int panX = 4;
    const int panY = BTN_STRIP;
    const int panW = (SCREEN_W - bW - 2) - bW - 4 - panX;  // same right edge as _drawTerminal
    const int panH = GROUND_Y - BTN_STRIP;

    c.fillRect(panX, panY, panW, panH, Palette::TermBg);
    c.drawRect(panX, panY, panW, panH, Palette::TermBdr);

    // ── Lock icon ─────────────────────────────────────────────
    const int iconCx = panX + panW / 2;
    const int iconCy = panY + panH / 4;

    // Shackle bars
    c.fillRect(iconCx - 7, iconCy - 20, 4, 20, Palette::BubbleBdr);
    c.fillRect(iconCx + 3, iconCy - 20, 4, 20, Palette::BubbleBdr);
    c.fillRect(iconCx - 7, iconCy - 20, 14, 5,  Palette::BubbleBdr);
    // Shackle hollow
    c.fillRect(iconCx - 3, iconCy - 15, 6, 15, Palette::TermBg);

    // Body
    c.fillRect(iconCx - 13, iconCy, 26, 18, Palette::ExpFill);
    c.drawRect(iconCx - 13, iconCy, 26, 18, Palette::White);

    // Keyhole
    c.fillCircle(iconCx, iconCy + 7, 4, Palette::TermBg);
    c.fillRect(iconCx - 2, iconCy + 7, 4, 7, Palette::TermBg);

    // ── Buttons ───────────────────────────────────────────────
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextDatum(lgfx::middle_center);

    const int btnX  = panX + 8;
    const int btnW  = panW - 16;
    const int btnH  = 26;
    const int btn1Y = panY + panH / 2 + 4;
    const int btn2Y = btn1Y + btnH + 8;

    // Crack Eapol — gold border
    c.fillRect(btnX, btn1Y, btnW, btnH, Palette::TermBg);
    c.drawRect(btnX, btn1Y, btnW, btnH, Palette::ExpFill);
    c.setTextColor(Palette::ExpFill, Palette::TermBg);
    c.drawString("Crack Eapol", btnX + btnW / 2, btn1Y + btnH / 2);

    // Cancel — green border
    c.fillRect(btnX, btn2Y, btnW, btnH, Palette::TermBg);
    c.drawRect(btnX, btn2Y, btnW, btnH, Palette::TermBdr);
    c.setTextColor(Palette::TermText, Palette::TermBg);
    c.drawString("Cancel", btnX + btnW / 2, btn2Y + btnH / 2);
}

// ── Main loop ─────────────────────────────────────────────────

void App::update() {
    M5.update();
    uint32_t ms = millis();

    _pet.update(ms);
    _handleTouch(ms);
    _updateHunting(ms);

    _canvas->fillScreen(Palette::SkyBot);
    _bg.draw(*_canvas);
    _pet.draw(*_canvas);
    _drawHud(*_canvas);
    if (_huntState == HuntState::Menu && _menuReady)
        _drawMenu(*_canvas);
    else
        _drawTerminal(*_canvas);
    _canvas->pushSprite(0, 0);
}
