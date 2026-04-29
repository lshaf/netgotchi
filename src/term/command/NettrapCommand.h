#pragma once
#include "MenuCommand.h"
#include <cstdint>

class WiFiHunter;

class NettrapCommand : public MenuCommand {
public:
    const char* label() const override { return "nettrap"; }
    void execute(IMenuHost& host) override {
        host.cmdPush("service nettrap start");
        host.startService(this);
    }

    void init(WiFiHunter* hunter);
    void startHardware()              override;
    void stopService(IMenuHost& host) override;
    void update(IMenuHost& host, uint32_t ms) override;
    void clearState() override;
    Virus::State virusState() const override { return Virus::State::Trap; }

private:
    WiFiHunter* _hunter = nullptr;

    uint32_t _lastCaptureCount    = 0;
    uint32_t _lastApFoundCount    = 0;
    uint32_t _lastEapolEventCount = 0;

    uint8_t  _channel       = 1;
    uint32_t _nextChannelMs = 0;
    bool     _waiting       = false;
};
