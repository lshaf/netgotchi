#include "NethuntCommand.h"
#include "../../net/WiFiHunter.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

static constexpr uint32_t HUNT_DWELL_MS  = 5000;
static constexpr uint32_t DEAUTH_WAIT_MS = 2000;
static constexpr uint8_t  DEAUTH_ROUNDS  = 3;

void NethuntCommand::init(WiFiHunter* hunter) { _hunter = hunter; }

void NethuntCommand::startHardware() {
    if (!_hunter) return;
    _channel   = 1;
    _huntPhase = HuntPhase::SetChannel;
    _hunter->resume();
    _lastCaptureCount        = _hunter->captureCount();
    _lastApFoundCount        = _hunter->apFoundCount();
    _lastDeauthTargetCount   = _hunter->deauthTargetCount();
    _lastEapolEventCount     = _hunter->eapolEventCount();
    _lastExternalDeauthCount = _hunter->externalDeauthCount();
}

void NethuntCommand::stopService(IMenuHost& host) {
    if (_hunter) {
        _hunter->pause();
        _hunter->cleanupInvalidPcaps();
    }
    host.cmdPush("service nethunt stop");
}

void NethuntCommand::clearState() {
    _lastCaptureCount        = 0;
    _lastApFoundCount        = 0;
    _lastDeauthTargetCount   = 0;
    _lastEapolEventCount     = 0;
    _lastExternalDeauthCount = 0;
    _channel      = 1;
    _deauthApIdx  = 0;
    _deauthRound  = 0;
    _pauseUntilMs = 0;
    _huntPhase    = HuntPhase::SetChannel;
}

static int findApOnChannel(const WiFiHunter* hunter, uint8_t ch, uint8_t startIdx) {
    uint8_t count = hunter->apCount();
    for (uint8_t i = startIdx; i < count; i++) {
        const WiFiHunter::ApInfo* ap = hunter->apInfoAt(i);
        if (!ap || ap->channel != ch || ap->validated || ap->deauthCount >= DEAUTH_ROUNDS)
            continue;
        return (int)i;
    }
    return -1;
}

void NethuntCommand::update(IMenuHost& host, uint32_t ms) {
    if (!_hunter || host.menuIsOpen()) return;

    _hunter->update(ms);

    // ── Event display ─────────────────────────────────────────
    uint32_t afc = _hunter->apFoundCount();
    if (afc > _lastApFoundCount) {
        _lastApFoundCount = afc;
        const char* ssid = _hunter->lastFoundSsid();
        char buf[48];
        snprintf(buf, sizeof(buf), "detected %.32s", (ssid && ssid[0]) ? ssid : "<hidden>");
        host.outPush(buf);
    }

    uint32_t dtc = _hunter->deauthTargetCount();
    if (dtc > _lastDeauthTargetCount) {
        _lastDeauthTargetCount = dtc;
        const char* dsid = _hunter->lastDeauthSsid();
        char buf[48];
        snprintf(buf, sizeof(buf), "deauth %.32s", (dsid && dsid[0]) ? dsid : "??");
        host.cmdPush(buf);
    }

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

    case HuntPhase::SetChannel:
        _hunter->setChannel(_channel);
        {
            char buf[24];
            snprintf(buf, sizeof(buf), "setchannel %d", _channel);
            host.cmdPush(buf);
        }
        _pauseUntilMs = ms + HUNT_DWELL_MS;
        _huntPhase    = HuntPhase::WaitChannel;
        break;

    case HuntPhase::WaitChannel:
        if (ms < _pauseUntilMs) break;
        _huntPhase = HuntPhase::CheckChannel;
        break;

    case HuntPhase::CheckChannel: {
        int idx = findApOnChannel(_hunter, _channel, 0);
        if (idx < 0) {
            _hunter->resetDeauthCountsOnChannel(_channel);
            _channel   = (_channel % 13) + 1;
            _huntPhase = HuntPhase::SetChannel;
        } else {
            _deauthApIdx = (uint8_t)idx;
            _deauthRound = 1;
            _hunter->deauthApByIdx(_deauthApIdx);
            _pauseUntilMs = ms + DEAUTH_WAIT_MS;
            _huntPhase    = HuntPhase::Deauthing;
        }
        break;
    }

    case HuntPhase::Deauthing:
        if (ms < _pauseUntilMs) break;
        {
            const WiFiHunter::ApInfo* ap = _hunter->apInfoAt(_deauthApIdx);
            if (ap && !ap->validated && _deauthRound < DEAUTH_ROUNDS) {
                _hunter->deauthApByIdx(_deauthApIdx);
                _deauthRound++;
                _pauseUntilMs = ms + DEAUTH_WAIT_MS;
            } else {
                _huntPhase = HuntPhase::NextWifi;
            }
        }
        break;

    case HuntPhase::NextWifi: {
        int idx = findApOnChannel(_hunter, _channel, 0);
        if (idx >= 0) {
            _deauthApIdx = (uint8_t)idx;
            _deauthRound = 1;
            _hunter->deauthApByIdx(_deauthApIdx);
            _pauseUntilMs = ms + DEAUTH_WAIT_MS;
            _huntPhase    = HuntPhase::Deauthing;
        } else {
            _hunter->resetDeauthCountsOnChannel(_channel);
            _channel   = (_channel % 13) + 1;
            _huntPhase = HuntPhase::SetChannel;
        }
        break;
    }
    }
}
