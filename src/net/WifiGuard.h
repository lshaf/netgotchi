#pragma once
#include <cstdint>
#include "esp_wifi.h"

class WifiGuard {
public:
    void init();
    void update(uint32_t ms);
    void pause() { esp_wifi_set_promiscuous(false); }

    uint8_t     channel()          const { return _channel; }
    uint32_t    deauthCount()      const { return _deauthCount; }
    uint32_t    beaconFloodCount() const { return _beaconFloodCount; }
    uint32_t    evilTwinCount()    const { return _evilTwinCount; }
    const char* lastDeauthSsid()   const { return _lastDeauthSsid; }
    const char* lastFloodSsid()    const { return _lastFloodSsid; }
    const char* lastEvilTwinSsid() const { return _lastEvilTwinSsid; }

private:
    static constexpr int      MAX_APS             = 24;
    static constexpr int      DEAUTH_RING          = 32;
    static constexpr int      BEACON_RING          = 128;
    static constexpr int      BEACON_FLOOD_THRESH  = 50;
    static constexpr uint32_t HOP_MS              = 1000;

    struct ApEntry {
        uint8_t  bssid[6]     = {};
        char     ssid[33]     = {};
        uint16_t beaconCount  = 0;
        uint16_t beaconRate   = 0;
        bool     floodAlerted = false;
        uint32_t deauthMs     = 0;
    };

    struct DeauthEv { uint8_t bssid[6]; bool isDisassoc; };
    struct BeaconEv { uint8_t bssid[6]; char ssid[33]; };

    static WifiGuard* _instance;
    static void _promiscCb(void*, wifi_promiscuous_pkt_type_t);

    void     _flush();
    void     _updateRates();
    void     _checkEvilTwins();
    ApEntry* _findAp(const uint8_t* bssid);
    ApEntry* _registerAp(const uint8_t* bssid);

    DeauthEv     _deauthRing[DEAUTH_RING]{};
    volatile int _dHead = 0, _dTail = 0;

    BeaconEv     _beaconRing[BEACON_RING]{};
    volatile int _bHead = 0, _bTail = 0;

    ApEntry  _aps[MAX_APS]{};
    uint8_t  _apCount = 0;

    uint8_t  _channel = 1;
    uint32_t _hopMs   = 0;

    uint32_t _deauthCount      = 0;
    uint32_t _beaconFloodCount = 0;
    uint32_t _evilTwinCount    = 0;
    char     _lastDeauthSsid[33]   = {};
    char     _lastFloodSsid[33]    = {};
    char     _lastEvilTwinSsid[33] = {};
};
