#pragma once
#include <cstdint>
#include <SD.h>
#include "esp_wifi.h"

class WiFiHunter {
public:
    struct ApInfo {
        uint8_t  bssid[6];
        char     ssid[33];
        uint8_t  channel;
        bool     validated;
        uint8_t  deauthCount;
        bool     pcapCreated;
        uint8_t  anonce[32];
        uint8_t  staMacM1[6];
        uint8_t  staMacM2[6];
        bool     hasAnonce;
        bool     hasM2;
        uint32_t deauthDetectedMs;
    };

    void init();
    void update(uint32_t ms);
    void clearFindings(uint32_t ms);
    void pause()  { esp_wifi_set_promiscuous(false); }
    void resume() { esp_wifi_set_promiscuous_rx_cb(_promiscCb); esp_wifi_set_promiscuous(true); }
    void setChannel(uint8_t ch);
    void deauthApByIdx(uint8_t idx);
    void deauthAllOnChannel(uint8_t ch);
    void resetDeauthCountsOnChannel(uint8_t ch);
    void registerApFromScan(const uint8_t* bssid, const char* ssid, uint8_t ch);
    bool pcapIsComplete(const char* path);

    uint8_t  channel()                   const { return _channel; }
    uint8_t  apCount()                   const { return _apCount; }
    uint32_t captureCount()              const { return _captureCount; }
    uint32_t apFoundCount()              const { return _apFoundCount; }
    uint32_t deauthBurstCount()          const { return _deauthBurstCount; }
    uint32_t deauthTargetCount()         const { return _deauthTargetCount; }
    uint32_t eapolEventCount()           const { return _eapolEventCount; }
    uint32_t externalDeauthCount()       const { return _externalDeauthCount; }
    const char*    lastFoundSsid()          const { return _lastFoundSsid; }
    const char*    lastDeauthSsid()         const { return _lastDeauthSsid; }
    int            lastEapolMsg()           const { return _lastEapolMsg; }
    const char*    lastEapolSsid()          const { return _lastEapolSsid; }
    const char*    lastCapturePath()        const { return _lastCapturePath; }
    const char*    lastExternalDeauthSsid() const { return _lastExternalDeauthSsid; }
    const ApInfo*  apInfoAt(uint8_t idx)    const { return (idx < _apCount) ? &_aps[idx] : nullptr; }

private:
    static constexpr int MAX_FRAME          = 400;
    static constexpr int RING_SIZE          = 32;
    static constexpr int MAX_PENDING_PER_AP = 4;
    static constexpr int MAX_APS            = 20;
    static constexpr int DEAUTH_BURSTS      = 10;

    struct RawFrame {
        uint8_t  data[MAX_FRAME];
        uint16_t len;
        uint8_t  channel;
        bool     isBeacon;
        bool     isDeauth;
    };

    static WiFiHunter* _instance;
    static void _promiscCb(void* buf, wifi_promiscuous_pkt_type_t type);

    void _flush();
    void _processBeacon(const RawFrame& f);
    void _processEapol(const RawFrame& f);
    void _processDeauth(const RawFrame& f);
    void _deauthAp(const ApInfo& ap);

    ApInfo* _findAp(const uint8_t* bssid);
    ApInfo* _registerAp(const uint8_t* bssid);

    void _writePcapHeader(File& f);
    void _appendPcapFrame(File& f, const uint8_t* data, uint16_t len);
    void _buildFilePath(char* buf, int bufLen, const ApInfo& ap);
    bool _pcapIsComplete(const ApInfo& ap);
    bool _pcapIsCompletePath(const char* path);

    void _ensurePcapWithBeacon(ApInfo& ap, int idx);
    void _flushPendingEapol  (ApInfo& ap, int idx);
    void _bufferEapol(int idx, const uint8_t* data, uint16_t len);
    void _validateEapol(ApInfo& ap, const uint8_t* pay, uint16_t flen);

    static int _parseEapolMsg(const uint8_t* data, uint16_t len, int* snapOffOut);

    // ── State ─────────────────────────────────────────────────
    uint8_t  _channel   = 1;
    uint16_t _deauthSeq = 0;

    // ── AP table ──────────────────────────────────────────────
    ApInfo   _aps[MAX_APS]{};
    uint8_t  _apCount = 0;

    uint8_t  _beaconData[MAX_APS][MAX_FRAME]{};
    uint16_t _beaconLen[MAX_APS]{};

    uint8_t  _pendingEapol   [MAX_APS][MAX_PENDING_PER_AP][MAX_FRAME]{};
    uint16_t _pendingEapolLen[MAX_APS][MAX_PENDING_PER_AP]{};
    uint8_t  _pendingEapolCount[MAX_APS]{};

    // ── Event counters (polled by commands) ───────────────────
    uint32_t _captureCount          = 0;
    uint32_t _apFoundCount          = 0;
    uint32_t _deauthBurstCount      = 0;
    uint32_t _deauthTargetCount     = 0;
    uint32_t _eapolEventCount       = 0;
    uint32_t _externalDeauthCount   = 0;
    int      _lastEapolMsg          = 0;
    char     _lastFoundSsid[33]          = {};
    char     _lastDeauthSsid[33]         = {};
    char     _lastEapolSsid[33]          = {};
    char     _lastCapturePath[64]        = {};
    char     _lastExternalDeauthSsid[33] = {};

    // ── SPSC ring ─────────────────────────────────────────────
    RawFrame      _ring[RING_SIZE]{};
    volatile int  _ringHead    = 0;
    volatile int  _ringTail    = 0;
    volatile bool _skipBeacons = false;
};
