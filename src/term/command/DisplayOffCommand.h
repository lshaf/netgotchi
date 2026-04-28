#pragma once
#include "MenuCommand.h"

class DisplayOffCommand : public MenuCommand {
public:
    const char* label() const override { return "displayoff"; }
    void execute(IMenuHost& host) override { host.openSubMenu(this); }

    int         subCount()              const override;
    const char* subLabel(int idx)       const override;
    bool        subIsActive(int idx)    const override;
    int         subItemH()              const override { return 14; }
    void onSubSelect(IMenuHost& host, int idx) override;
};
