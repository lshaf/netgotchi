#pragma once
#include "MenuCommand.h"
#include <cstdint>

class WiFiHunter;

class NethuntCommand : public MenuCommand {
public:
    enum class HuntPhase : uint8_t {
        SetChannel,
        WaitChannel,
        CheckChannel,
        Deauthing,
        NextWifi,
    };

    const char* label() const override { return "nethunt"; }
    void execute(IMenuHost& host) override {
        host.cmdPush("service nethunt start");
        host.startService(this);
    }

    void init(WiFiHunter* hunter);
    void startHardware()              override;
    void stopService(IMenuHost& host) override;
    void update(IMenuHost& host, uint32_t ms) override;
    void clearState() override;
    Virus::State virusState() const override { return Virus::State::Active; }

private:
    WiFiHunter* _hunter = nullptr;

    uint32_t _lastCaptureCount        = 0;
    uint32_t _lastApFoundCount        = 0;
    uint32_t _lastDeauthTargetCount   = 0;
    uint32_t _lastEapolEventCount     = 0;
    uint32_t _lastExternalDeauthCount = 0;

    uint8_t   _channel      = 1;
    uint8_t   _deauthApIdx  = 0;
    uint8_t   _deauthRound  = 0;
    uint32_t  _pauseUntilMs = 0;
    HuntPhase _huntPhase    = HuntPhase::SetChannel;
};
