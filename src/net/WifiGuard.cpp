#include "WifiGuard.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <cstring>
#include <cstdio>

WifiGuard* WifiGuard::_instance = nullptr;

void WifiGuard::init() {
    _instance = this;

    _apCount = 0;
    memset(_aps, 0, sizeof(_aps));
    _dHead = 0; _dTail = 0;
    _bHead = 0; _bTail = 0;
    _deauthCount = _beaconFloodCount = _evilTwinCount = 0;
    _lastDeauthSsid[0] = _lastFloodSsid[0] = _lastEvilTwinSsid[0] = '\0';
    _channel = 1;
    _hopMs   = 0;

    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_set_promiscuous_rx_cb(&WifiGuard::_promiscCb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);


}

void WifiGuard::update(uint32_t ms) {
    _flush();

    if (ms - _hopMs >= HOP_MS) {
        _hopMs = ms;
        _updateRates();
        _checkEvilTwins();
        _channel = (_channel % 13) + 1;
        esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
    }
}

void WifiGuard::_promiscCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!_instance || buf == nullptr) return;
    if (type != WIFI_PKT_MGMT) return;

    const auto*    pkt = static_cast<wifi_promiscuous_pkt_t*>(buf);
    const uint8_t* pay = pkt->payload;
    const size_t   len = pkt->rx_ctrl.sig_len;
    if (len < 4) return;

    const uint8_t fc_sub  = (pay[0] >> 4) & 0x0F;
    const uint8_t fc_type = (pay[0] >> 2) & 0x03;
    if (fc_type != 0) return;

    // Deauth (0xC) or Disassoc (0xA) — addr2 = SA = frame sender BSSID
    if ((fc_sub == 0xC || fc_sub == 0xA) && len >= 16) {
        int next = (_instance->_dHead + 1) % DEAUTH_RING;
        if (next != _instance->_dTail) {
            memcpy(_instance->_deauthRing[_instance->_dHead].bssid, pay + 10, 6);
            _instance->_deauthRing[_instance->_dHead].isDisassoc = (fc_sub == 0xA);
            _instance->_dHead = next;
        }
        return;
    }

    // Beacon (8) — SSID + rate tracking
    if (fc_sub == 8 && len >= 36) {
        int next = (_instance->_bHead + 1) % BEACON_RING;
        if (next != _instance->_bTail) {
            BeaconEv& ev = _instance->_beaconRing[_instance->_bHead];
            memcpy(ev.bssid, pay + 16, 6);
            ev.ssid[0] = '\0';
            size_t pos = 36;
            while (pos + 2 <= len) {
                uint8_t id = pay[pos], el = pay[pos + 1];
                if (pos + 2 + el > len) break;
                if (id == 0 && el > 0 && el <= 32) {
                    memcpy(ev.ssid, pay + pos + 2, el);
                    ev.ssid[el] = '\0';
                    break;
                }
                pos += 2 + el;
            }
            _instance->_bHead = next;
        }
    }
}

void WifiGuard::_flush() {
    while (_dTail != _dHead) {
        const DeauthEv& ev = _deauthRing[_dTail];
        ApEntry* ap = _findAp(ev.bssid);
        if (!ap) ap = _registerAp(ev.bssid);
        if (ap) {
            uint32_t now = millis();
            if (now - ap->deauthMs >= 5000) {
                ap->deauthMs = now;
                _deauthCount++;
                if (ap->ssid[0]) {
                    strncpy(_lastDeauthSsid, ap->ssid, 32);
                } else {
                    snprintf(_lastDeauthSsid, sizeof(_lastDeauthSsid),
                             "%02X:%02X:%02X", ev.bssid[0], ev.bssid[1], ev.bssid[2]);
                }
                _lastDeauthSsid[32] = '\0';
            }
        }
        _dTail = (_dTail + 1) % DEAUTH_RING;
    }

    while (_bTail != _bHead) {
        const BeaconEv& ev = _beaconRing[_bTail];
        ApEntry* ap = _findAp(ev.bssid);
        if (!ap) {
            ap = _registerAp(ev.bssid);
            if (ap && ev.ssid[0]) strncpy(ap->ssid, ev.ssid, 32);
        } else if (ap->ssid[0] == '\0' && ev.ssid[0]) {
            strncpy(ap->ssid, ev.ssid, 32);
        }
        if (ap) ap->beaconCount++;
        _bTail = (_bTail + 1) % BEACON_RING;
    }
}

void WifiGuard::_updateRates() {
    _beaconFloodCount = 0;
    for (int i = 0; i < _apCount; i++) {
        ApEntry& ap = _aps[i];
        ap.beaconRate  = ap.beaconCount;
        ap.beaconCount = 0;
        if (ap.beaconRate >= BEACON_FLOOD_THRESH) {
            _beaconFloodCount++;
            if (!ap.floodAlerted) {
                ap.floodAlerted = true;
                strncpy(_lastFloodSsid, ap.ssid[0] ? ap.ssid : "??", 32);
                _lastFloodSsid[32] = '\0';
            }
        } else {
            ap.floodAlerted = false;
        }
    }
}

void WifiGuard::_checkEvilTwins() {
    _evilTwinCount = 0;
    for (int i = 0; i < _apCount; i++) {
        if (_aps[i].ssid[0] == '\0') continue;
        for (int j = i + 1; j < _apCount; j++) {
            if (_aps[j].ssid[0] == '\0') continue;
            if (strcmp(_aps[i].ssid, _aps[j].ssid) == 0 &&
                memcmp(_aps[i].bssid, _aps[j].bssid, 6) != 0) {
                _evilTwinCount++;
                strncpy(_lastEvilTwinSsid, _aps[i].ssid, 32);
                _lastEvilTwinSsid[32] = '\0';
            }
        }
    }
}

WifiGuard::ApEntry* WifiGuard::_findAp(const uint8_t* bssid) {
    for (int i = 0; i < _apCount; i++)
        if (memcmp(_aps[i].bssid, bssid, 6) == 0) return &_aps[i];
    return nullptr;
}

WifiGuard::ApEntry* WifiGuard::_registerAp(const uint8_t* bssid) {
    if (_apCount >= MAX_APS) return nullptr;
    ApEntry& ap = _aps[_apCount++];
    memset(&ap, 0, sizeof(ap));
    memcpy(ap.bssid, bssid, 6);
    return &ap;
}
