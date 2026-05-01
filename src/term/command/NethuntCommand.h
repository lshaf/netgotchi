#pragma once
#include "MenuCommand.h"
#include <cstdint>

class WiFiHunter;

class NethuntCommand : public MenuCommand {
public:
    enum class HuntPhase : uint8_t {
        StartScan,    // initiate async WiFi scan
        Scanning,     // wait for scan completion
        ScanDone,     // process results, build channel map
        SetChannel,   // switch radio to target channel
        DeauthRound,  // fire deauths at all APs on channel
        WaitRound,    // 5s gap between deauth rounds
        NextChannel,  // advance channel or go to exhaust
        Exhaust,      // 60s cooldown before next scan
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
    Virus::State virusState() const override {
        return (_huntPhase == HuntPhase::Exhaust) ? Virus::State::Sleep : Virus::State::Active;
    }

private:
    WiFiHunter* _hunter = nullptr;

    uint32_t _lastCaptureCount        = 0;
    uint32_t _lastEapolEventCount     = 0;
    uint32_t _lastExternalDeauthCount = 0;

    uint8_t   _channel      = 1;
    uint16_t  _channelMask  = 0;    // bits 1-13: channels with pending APs
    uint8_t   _deauthRound  = 0;
    uint32_t  _pauseUntilMs = 0;
    HuntPhase _huntPhase    = HuntPhase::StartScan;
};