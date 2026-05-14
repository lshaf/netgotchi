#pragma once
#include "MenuCommand.h"
#include "../../net/WebFileServer.h"
#include <WiFi.h>
#include <cstdio>

class WebServerCommand : public MenuCommand {
public:
    const char* label() const override { return "webserver"; }

    void execute(IMenuHost& host) override {
        host.cmdPush("service webserver start");
        host.startService(this);
        char buf[52];
        snprintf(buf, sizeof(buf), "%s  %s.local",
                 WiFi.softAPIP().toString().c_str(),
                 WebFileServer::MDNS_HOST);
        host.outPush(buf);
    }

    void startHardware() override {
        // Re-broadcast the AP visibly. WiFiHunter::init() keeps it hidden by default.
        WiFi.softAP(WebFileServer::AP_SSID, nullptr, 1, /*ssid_hidden=*/0);
    }

    void stopService(IMenuHost& host) override {
        // Hide the AP again instead of tearing it down — hunter/deauth still
        // need APSTA mode active for raw-frame injection.
        WiFi.softAP(WebFileServer::AP_SSID, nullptr, 1, /*ssid_hidden=*/1);
        host.cmdPush("service webserver stop");
    }

    void update(IMenuHost&, uint32_t) override {}
    void clearState() override {}
    Virus::State virusState() const override { return Virus::State::Idle; }
};
