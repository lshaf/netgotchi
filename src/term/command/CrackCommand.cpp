#include "CrackCommand.h"
#include <SD.h>
#include <cstring>
#include <cstdio>

void CrackCommand::execute(IMenuHost& host) {
    if (!_pcapLoaded) {
        _loadPcapList();
        _pcapLoaded = true;
    }
    _state = kPcap;
    host.openSubMenu(this);
}

int CrackCommand::subCount() const {
    if (_state == kPcap) return _fileCount + 1;          // files + [back]
    return _fileCount + 2;                                // files + [built in] + [back]
}

const char* CrackCommand::subLabel(int idx) const {
    if (_state == kPcap) {
        if (idx < _fileCount) return _fileNames[idx];
        return "back";
    }
    // kDict
    if (idx < _fileCount) return _fileNames[idx];
    if (idx == _fileCount) return "built in";
    return "back";
}

const char* CrackCommand::inputHint() const {
    if (_state == kPcap) {
        strncpy(_hint, "crack", sizeof(_hint) - 1);
    } else {
        snprintf(_hint, sizeof(_hint), "crack %.42s", _basename(_selPcap));
    }
    return _hint;
}

void CrackCommand::onSubSelect(IMenuHost& host, int idx) {
    if (_state == kPcap) {
        if (idx < _fileCount) {
            strncpy(_selPcap, _filePaths[idx], sizeof(_selPcap) - 1);
            _loadDictList();
            _state = kDict;
        } else {
            host.menuBack();
        }
        return;
    }

    // kDict — build full command, push it, then trigger crack
    const char* dictPath = nullptr;
    const char* dictName = nullptr;
    if (idx < _fileCount) {
        dictPath = _filePaths[idx];
        dictName = _basename(_filePaths[idx]);
    } else if (idx == _fileCount) {
        dictPath = "builtin";
        dictName = "builtin";
    } else {
        _pcapLoaded = false;  // invalidate so next execute() rescans
        _state = kPcap;
        return;
    }

    char cmd[52];
    snprintf(cmd, sizeof(cmd), "crack %.24s %.20s", _basename(_selPcap), dictName);
    host.cmdPush(cmd);
    host.startCrack(_selPcap, dictPath);
    host.menuClose();
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
