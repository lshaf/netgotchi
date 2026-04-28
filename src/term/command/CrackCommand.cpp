#include "CrackCommand.h"
#include <SD.h>
#include <cstring>
#include <cstdio>

// ── execute ───────────────────────────────────────────────────

void CrackCommand::execute(IMenuHost& host) {
    _state = kMenu;
    host.openSubMenu(this);
}

// ── Sub-menu geometry ─────────────────────────────────────────

int CrackCommand::subCount() const {
    switch (_state) {
        case kMenu: return _canStart() ? 3 : 2;        // [pcap] [dict] ([start])
        case kPcap: return _fileCount + 1;              // files + [back]
        case kDict: return _fileCount + 2;              // files + [built in] + [back]
    }
    return 0;
}

int CrackCommand::subItemH() const {
    return (_state == kMenu) ? 18 : 14;
}

// ── Labels ────────────────────────────────────────────────────

const char* CrackCommand::subLabel(int idx) const {
    if (_state == kMenu) {
        if (idx == 0) {
            if (_selPcap[0])
                snprintf(_lbl0, sizeof(_lbl0), "pcap: %.38s", _basename(_selPcap));
            else
                snprintf(_lbl0, sizeof(_lbl0), "pcap: (none)");
            return _lbl0;
        }
        if (idx == 1) {
            if (_selDict[0] == '\0')
                snprintf(_lbl1, sizeof(_lbl1), "dict: (none)");
            else if (strcmp(_selDict, "builtin") == 0)
                snprintf(_lbl1, sizeof(_lbl1), "dict: built in");
            else
                snprintf(_lbl1, sizeof(_lbl1), "dict: %.38s", _basename(_selDict));
            return _lbl1;
        }
        return "start";
    }

    if (_state == kPcap) {
        if (idx < _fileCount) return _fileNames[idx];
        return "back";
    }

    // kDict
    if (idx < _fileCount) return _fileNames[idx];
    if (idx == _fileCount) return "built in";
    return "back";
}

// ── Selection ─────────────────────────────────────────────────

void CrackCommand::onSubSelect(IMenuHost& host, int idx) {
    if (_state == kMenu) {
        if (idx == 0) { _loadPcapList(); _state = kPcap; }
        else if (idx == 1) { _loadDictList(); _state = kDict; }
        else if (idx == 2 && _canStart()) {
            host.startCrack(_selPcap, _selDict);
            host.menuClose();
        }
        return;
    }

    if (_state == kPcap) {
        if (idx < _fileCount)
            strncpy(_selPcap, _filePaths[idx], sizeof(_selPcap) - 1);
        _state = kMenu;
        return;
    }

    // kDict
    if (idx < _fileCount) {
        strncpy(_selDict, _filePaths[idx], sizeof(_selDict) - 1);
    } else if (idx == _fileCount) {
        strncpy(_selDict, "builtin", sizeof(_selDict) - 1);
    }
    _state = kMenu;
}

// ── File listing ──────────────────────────────────────────────

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
