#pragma once
#include "MenuCommand.h"
#include <cstring>

class CrackCommand : public MenuCommand {
public:
    const char* label() const override { return "crack"; }
    void execute(IMenuHost& host) override;

    int         subCount()           const override;
    const char* subLabel(int idx)    const override;
    bool        subIsActive(int idx) const override { (void)idx; return false; }
    int         subItemH()           const override;
    void onSubSelect(IMenuHost& host, int idx) override;

private:
    enum State : uint8_t { kMenu, kPcap, kDict };
    static constexpr int MAX_FILES = 10;

    State _state = kMenu;
    char  _selPcap[64] = {};   // full path, empty = none
    char  _selDict[64] = {};   // full path or "builtin", empty = none

    char  _filePaths[MAX_FILES][64] = {};
    char  _fileNames[MAX_FILES][32] = {};
    int   _fileCount = 0;

    mutable char _lbl0[48] = {};  // label buffer for menu item 0
    mutable char _lbl1[48] = {};  // label buffer for menu item 1

    void _loadPcapList();
    void _loadDictList();

    bool _canStart() const { return _selPcap[0] != '\0' && _selDict[0] != '\0'; }

    static const char* _basename(const char* path) {
        const char* s = strrchr(path, '/');
        return s ? s + 1 : path;
    }
};
