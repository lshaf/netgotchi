#include "NettrapCommand.h"
#include "../../net/WiFiHunter.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

static constexpr uint32_t TRAP_DWELL_MS = 10000;

void NettrapCommand::init(WiFiHunter* hunter) { _hunter = hunter; }

void NettrapCommand::startHardware() {
    if (!_hunter) return;
    _channel = 1;
    _waiting = false;
    _hunter->resume();
    _lastCaptureCount    = _hunter->captureCount();
    _lastApFoundCount    = _hunter->apFoundCount();
    _lastEapolEventCount = _hunter->eapolEventCount();
}

void NettrapCommand::stopService(IMenuHost& host) {
    if (_hunter) _hunter->pause();
    host.cmdPush("service nettrap stop");
}

void NettrapCommand::clearState() {
    _lastCaptureCount    = 0;
    _lastApFoundCount    = 0;
    _lastEapolEventCount = 0;
    _channel             = 1;
    _nextChannelMs       = 0;
    _waiting             = false;
}

void NettrapCommand::update(IMenuHost& host, uint32_t ms) {
    if (!_hunter || host.menuIsOpen()) return;

    _hunter->update(ms);

    uint32_t afc = _hunter->apFoundCount();
    if (afc > _lastApFoundCount) {
        _lastApFoundCount = afc;
        const char* ssid = _hunter->lastFoundSsid();
        char buf[48];
        snprintf(buf, sizeof(buf), "detected %.32s", (ssid && ssid[0]) ? ssid : "<hidden>");
        host.outPush(buf);
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

    if (!_waiting) {
        _hunter->setChannel(_channel);
        char buf[24];
        snprintf(buf, sizeof(buf), "setchannel %d", _channel);
        host.cmdPush(buf);
        _nextChannelMs = ms + TRAP_DWELL_MS;
        _waiting       = true;
    } else if (ms >= _nextChannelMs) {
        _channel = (_channel % 13) + 1;
        _waiting = false;
    }
}
