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
    int         subItemH()           const override { return 14; }
    const char* inputHint()          const override;
    void onSubSelect(IMenuHost& host, int idx) override;

private:
    enum State : uint8_t { kPcap, kDict };
    static constexpr int MAX_FILES = 10;

    State _state = kPcap;
    char  _selPcap[64] = {};

    char  _filePaths[MAX_FILES][64] = {};
    char  _fileNames[MAX_FILES][32] = {};
    int   _fileCount = 0;
    mutable char _hint[52] = {};

    void _loadPcapList();
    void _loadDictList();

    static const char* _basename(const char* path) {
        const char* s = strrchr(path, '/');
        return s ? s + 1 : path;
    }
};
