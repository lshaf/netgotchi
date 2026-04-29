#pragma once
#include "MenuCommand.h"
#include <cstdint>

class WifiGuard;

class NetguardCommand : public MenuCommand {
public:
    const char* label() const override { return "netguard"; }
    void execute(IMenuHost& host) override {
        host.cmdPush("service netguard start");
        host.startService(this);
    }

    void init(WifiGuard* guard);
    void startHardware()              override;
    void stopService(IMenuHost& host) override;
    void update(IMenuHost& host, uint32_t ms) override;
    void clearState() override;
    Virus::State virusState() const override { return Virus::State::Guard; }

private:
    WifiGuard* _guard = nullptr;

    uint32_t _lastDeauthCount   = 0;
    uint32_t _lastFloodCount    = 0;
    uint32_t _lastEvilTwinCount = 0;
};
