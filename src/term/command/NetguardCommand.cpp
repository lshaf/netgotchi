#include "NetguardCommand.h"
#include "../../net/WifiGuard.h"
#include <cstdio>

void NetguardCommand::init(WifiGuard* guard) { _guard = guard; }

void NetguardCommand::startHardware() {
    if (_guard) _guard->init();
}

void NetguardCommand::stopService(IMenuHost& host) {
    if (_guard) _guard->pause();
    host.cmdPush("service netguard stop");
}

void NetguardCommand::clearState() {
    _lastDeauthCount   = 0;
    _lastFloodCount    = 0;
    _lastEvilTwinCount = 0;
}

void NetguardCommand::update(IMenuHost& host, uint32_t ms) {
    if (!_guard || host.menuIsOpen()) return;

    _guard->update(ms);

    uint32_t dc = _guard->deauthCount();
    if (dc > _lastDeauthCount) {
        _lastDeauthCount = dc;
        const char* sid = _guard->lastDeauthSsid();
        char buf[48];
        snprintf(buf, sizeof(buf), "alert deauth %.32s", (sid && sid[0]) ? sid : "??");
        host.outPush(buf);
        host.onDiscover();
    }

    uint32_t fc = _guard->beaconFloodCount();
    if (fc != _lastFloodCount) {
        _lastFloodCount = fc;
        if (fc > 0) {
            const char* sid = _guard->lastFloodSsid();
            char buf[48];
            snprintf(buf, sizeof(buf), "alert flood %.32s", (sid && sid[0]) ? sid : "??");
            host.outPush(buf);
            host.onDiscover();
        }
    }

    uint32_t tc = _guard->evilTwinCount();
    if (tc != _lastEvilTwinCount) {
        _lastEvilTwinCount = tc;
        if (tc > 0) {
            const char* sid = _guard->lastEvilTwinSsid();
            char buf[48];
            snprintf(buf, sizeof(buf), "alert twin %.32s", (sid && sid[0]) ? sid : "??");
            host.outPush(buf);
            host.onDiscover();
        }
    }
}
