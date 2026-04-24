#pragma once
#include <cstdint>
#include <SD.h>
#include "esp_wifi.h"

class WiFiHunter {
public:
    enum class Phase { Discovery, Attacking };

    struct ApInfo {
        uint8_t  bssid[6];
        char     ssid[33];
        uint8_t  channel;
        bool     validated;
        uint8_t  deauthCount;
        bool     pcapCreated;   // PCAP file has been created (header + beacon written)
        // EAPOL handshake pairing (M1/M3 ANonce + M2 SNonce from same STA)
        uint8_t  anonce[32];
        uint8_t  staMacM1[6];   // STA MAC seen in M1 or M3 (addr1 = DA)
        uint8_t  staMacM2[6];   // STA MAC seen in M2 (addr2 = SA)
        bool     hasAnonce;
        bool     hasM2;
    };

    void     init();
    void     update(uint32_t ms);

    Phase    phase()          const { return _phase; }
    uint8_t  channel()        const { return _channel; }
    bool     isDeauthing()    const;
    bool     isScanCooldown() const;
    int      targetCount()    const;
    uint32_t captureCount()   const { return _captureCount; }
    uint8_t  apCount()        const { return _apCount; }
    const char* lastSsid()    const { return (_apCount > 0) ? _aps[_apCount-1].ssid : ""; }

private:
    // ── ISR ring buffer ───────────────────────────────────────
    static constexpr int MAX_FRAME = 400;
    static constexpr int RING_SIZE = 32;

    struct RawFrame {
        uint8_t  data[MAX_FRAME];
        uint16_t len;
        uint8_t  channel;
        bool     isBeacon;
    };

    // ── AP tracking ───────────────────────────────────────────
    static constexpr int MAX_APS = 20;

    // ── Internal state ────────────────────────────────────────
    static WiFiHunter* _instance;
    static void _promiscCb(void* buf, wifi_promiscuous_pkt_type_t type);

    void _flush();
    void _processBeacon(const RawFrame& f);
    void _processEapol(const RawFrame& f);
    void _hopChannel();
    void _buildAttackChans();
    void _advanceAttackChannel(uint32_t ms);
    bool _channelDone(uint8_t ch) const;
    void _deauthAp(const ApInfo& ap);

    ApInfo* _findAp(const uint8_t* bssid);
    ApInfo* _registerAp(const uint8_t* bssid);

    // PCAP helpers
    void _writePcapHeader(File& f);
    void _appendPcapFrame(File& f, const uint8_t* data, uint16_t len);
    void _buildFilePath(char* buf, int bufLen, const ApInfo& ap);
    bool _pcapExists(const ApInfo& ap);

    // EAPOL message classifier: returns 1=M1, 2=M2, 3=M3, 4=M4, 0=unknown
    static int _parseEapolMsg(const uint8_t* data, uint16_t len, int* snapOffOut);

    // ── Discovery phase ───────────────────────────────────────
    static constexpr uint32_t DISCOVERY_DWELL_MS = 3000;
    static constexpr uint32_t SCAN_COOLDOWN_MS   = 5000;

    Phase    _phase   = Phase::Discovery;
    uint8_t  _channel = 1;
    uint32_t _phaseEntryMs        = 0;
    uint32_t _lastHopMs           = 0;
    uint32_t _lastDeauthMs        = 0;
    uint32_t _scanCooldownUntilMs = 0;
    uint16_t _deauthSeq           = 0;

    // ── Attack phase ──────────────────────────────────────────
    static constexpr uint32_t DEAUTH_INTERVAL_MS = 5000;
    static constexpr int      DEAUTH_BURSTS      = 10;
    static constexpr int      MAX_ATTACKS        = 10;

    uint8_t  _attackChans[13]{};
    uint8_t  _attackChanCount = 0;
    uint8_t  _attackChanIdx   = 0;

    // ── AP table ──────────────────────────────────────────────
    ApInfo   _aps[MAX_APS]{};
    uint8_t  _apCount = 0;

    // ── Beacon store for PCAP prefix ─────────────────────────
    uint8_t  _beaconData[MAX_APS][MAX_FRAME]{};
    uint16_t _beaconLen[MAX_APS]{};

    uint32_t _captureCount = 0;

    // ── SPSC ring ─────────────────────────────────────────────
    RawFrame     _ring[RING_SIZE]{};
    volatile int _ringHead    = 0;
    volatile int _ringTail    = 0;
    volatile bool _skipBeacons = false;  // true during attack to keep ring free for EAPOL
};
