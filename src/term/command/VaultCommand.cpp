#include "VaultCommand.h"
#include "../../hw/Hw.h"
#include <SD.h>
#include <cstring>
#include <cstdio>

void VaultCommand::execute(IMenuHost& host) {
    _loadList();
    host.openSubMenu(this);
}

int VaultCommand::subCount() const {
    return _fileCount + 1;
}

const char* VaultCommand::subLabel(int idx) const {
    if (idx < _fileCount) return _fileNames[idx];
    return "back";
}

void VaultCommand::onSubSelect(IMenuHost& host, int idx) {
    if (idx >= _fileCount) {
        host.menuBack();
        return;
    }

    static const char* kSep = "+----------+----------------+";
    char buf[56];
    char val[24];

    auto row = [&](const char* key, const char* v) {
        snprintf(buf, sizeof(buf), "| %-8s | %-14s |", key, v);
        host.outPush(buf);
    };

    snprintf(buf, sizeof(buf), "vault %s", _fileNames[idx]);
    host.menuClose();
    host.cmdPush(buf);

    File f = Hw::sd.open(_filePaths[idx], FILE_READ);
    if (f) {
        size_t n = f.readBytes(val, sizeof(val) - 1);
        val[n] = '\0';
        f.close();
    } else {
        strncpy(val, "read error", sizeof(val));
    }

    host.outPush(kSep);
    row("ssid", _ssidNames[idx]);
    row("pass", val);
    host.outPush(kSep);
}

void VaultCommand::_loadList() {
    _fileCount = 0;
    File dir = Hw::sd.open("/netgotchi/cracked");
    if (!dir) return;
    File f = dir.openNextFile();
    while (f && _fileCount < MAX_FILES) {
        if (!f.isDirectory()) {
            const char* full = f.name();
            const char* base = strrchr(full, '/');
            base = base ? base + 1 : full;
            int nl = (int)strlen(base);
            if (nl >= 6 && strcmp(base + nl - 5, ".pass") == 0) {
                snprintf(_filePaths[_fileCount], 64, "/netgotchi/cracked/%s", base);
                // filename label: base without ".pass"
                int name_len = nl - 5;
                if (name_len > 51) name_len = 51;
                memcpy(_fileNames[_fileCount], base, name_len);
                _fileNames[_fileCount][name_len] = '\0';
                // ssid: skip BSSID(12) + '_'
                const char* ssid = (nl > 13 && base[12] == '_') ? base + 13 : base;
                int ssid_len = (int)strlen(ssid) - 5; // strip ".pass"
                if (ssid_len > 32) ssid_len = 32;
                if (ssid_len > 0) {
                    memcpy(_ssidNames[_fileCount], ssid, ssid_len);
                    _ssidNames[_fileCount][ssid_len] = '\0';
                } else {
                    strncpy(_ssidNames[_fileCount], base, 32);
                    _ssidNames[_fileCount][32] = '\0';
                }
                _fileCount++;
            }
        }
        f.close();
        f = dir.openNextFile();
    }
    if (f) f.close();
    dir.close();
}
