#pragma once
#include "MenuCommand.h"

class VaultCommand : public MenuCommand {
public:
    const char* label() const override { return "vault"; }
    void execute(IMenuHost& host) override;

    int         subCount()           const override;
    const char* subLabel(int idx)    const override;
    bool        subIsActive(int idx) const override { (void)idx; return false; }
    int         subItemH()           const override { return 14; }
    const char* inputHint()          const override { return "vault"; }
    void        onSubSelect(IMenuHost& host, int idx) override;

private:
    static constexpr int MAX_FILES = 10;

    char _filePaths[MAX_FILES][64] = {};
    char _ssidNames[MAX_FILES][33] = {};
    int  _fileCount = 0;

    void _loadList();
};
