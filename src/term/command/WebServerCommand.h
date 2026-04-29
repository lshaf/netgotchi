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
        WiFi.softAP(WebFileServer::AP_SSID);
    }

    void stopService(IMenuHost& host) override {
        WiFi.softAPdisconnect(true);
        host.cmdPush("service webserver stop");
    }

    void update(IMenuHost&, uint32_t) override {}
    void clearState() override {}
    Virus::State virusState() const override { return Virus::State::Idle; }
};
