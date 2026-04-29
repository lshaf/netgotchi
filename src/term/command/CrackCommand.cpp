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
        _loadPcapList();
        _subState = kPcap;
        return;
    }

    char cmd[56];
    snprintf(cmd, sizeof(cmd), "crack %s %s", _basename(_selPcap), dictName);
    host.cmdPush(cmd);

    strncpy(_crackPcapPath, _selPcap, sizeof(_crackPcapPath) - 1);
    strncpy(_crackCtx.wordlistPath, dictPath, sizeof(_crackCtx.wordlistPath) - 1);
    host.menuClose();
    _pendingStart = true;
    host.setCurrentService(this);
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
                snprintf(_fileNames[_fileCount], 52, "%.51s", base);
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
            snprintf(_fileNames[_fileCount], 52, "%.51s", base);
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

bool CrackCommand::progressLine(char* buf, int len, uint32_t ms) const {
    if (_crackState != CrackState::Running) return false;
    uint32_t pct = (_crackCtx.fileSize > 0)
        ? (uint32_t)((uint64_t)_crackCtx.bytesDone * 100 / _crackCtx.fileSize)
        : 0;
    if (pct > 100) pct = 100;
    char bar[21]; int filled = (int)(20 * pct / 100);
    for (int i = 0; i < 20; i++) bar[i] = (i < filled) ? '#' : ' ';
    bar[20] = '\0';

    char speedBuf[10] = "";
    char etaBuf[10]   = "";
    uint32_t elapsed_s = (ms - _crackStartMs + 500) / 1000;
    if (elapsed_s > 0) {
        uint32_t wps = _crackCtx.tested / elapsed_s;
        if (wps >= 1000) snprintf(speedBuf, sizeof(speedBuf), " %luk/s", (unsigned long)(wps / 1000));
        else             snprintf(speedBuf, sizeof(speedBuf), " %lu/s",  (unsigned long)wps);

        uint32_t bps = (_crackCtx.bytesDone > 0) ? _crackCtx.bytesDone / elapsed_s : 0;
        if (bps > 0 && _crackCtx.fileSize > _crackCtx.bytesDone) {
            uint32_t eta = (_crackCtx.fileSize - _crackCtx.bytesDone) / bps;
            if (eta < 60) snprintf(etaBuf, sizeof(etaBuf), " %lus",       (unsigned long)eta);
            else          snprintf(etaBuf, sizeof(etaBuf), " %lum%02lus",  (unsigned long)(eta / 60), (unsigned long)(eta % 60));
        }
    }
    snprintf(buf, (size_t)len, "[%s] %lu%%%s%s", bar, (unsigned long)pct, speedBuf, etaBuf);
    return true;
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
    if (!fast_pcap_parse(_crackPcapPath, hs, &reason)) {
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

    uint32_t elapsed_s = (ms - _crackStartMs + 500) / 1000;
    uint32_t tried     = _crackCtx.tested;

    static const char* kSep = "+----------+----------------+";
    char buf[56];
    char val[24];
    auto row = [&](const char* key, const char* v) {
        snprintf(buf, sizeof(buf), "| %-8s | %-14s |", key, v);
        host.outPush(buf);
    };

    host.outPush(kSep);

    if (_crackCtx.found) {
        char savePath[80];
        char bssid[13];
        snprintf(bssid, sizeof(bssid), "%02X%02X%02X%02X%02X%02X",
                 _crackCtx.hs.ap[0], _crackCtx.hs.ap[1], _crackCtx.hs.ap[2],
                 _crackCtx.hs.ap[3], _crackCtx.hs.ap[4], _crackCtx.hs.ap[5]);
        SD.mkdir("/netgotchi/cracked");
        snprintf(savePath, sizeof(savePath), "/netgotchi/cracked/%.12s_%.32s.pass",
                 bssid, _crackCtx.hs.ssid);
        bool award = true;
        if (SD.exists(savePath)) {
            File rf = SD.open(savePath, FILE_READ);
            if (rf) {
                char existing[64] = {};
                int n = rf.readBytes(existing, (int)strlen(_crackCtx.foundPass) + 1);
                rf.close();
                if (n == (int)strlen(_crackCtx.foundPass) &&
                    memcmp(existing, _crackCtx.foundPass, n) == 0)
                    award = false;
            }
        }
        File pf = SD.open(savePath, FILE_WRITE);
        if (pf) { pf.print(_crackCtx.foundPass); pf.close(); }
        if (award) host.onCracked();
        row("ssid", _crackCtx.hs.ssid);
        row("pass", _crackCtx.foundPass);
    } else {
        row("result", _crackCtx.stop ? "interrupted" : "not found");
    }

    if (elapsed_s < 60)
        snprintf(val, sizeof(val), "%lus", (unsigned long)elapsed_s);
    else
        snprintf(val, sizeof(val), "%lum%02lus",
                 (unsigned long)(elapsed_s / 60), (unsigned long)(elapsed_s % 60));
    row("time", val);

    snprintf(val, sizeof(val), "%lu", (unsigned long)tried);
    row("tried", val);

    host.outPush(kSep);
    _crackState = CrackState::Idle;
    host.setCurrentService(nullptr);
}
