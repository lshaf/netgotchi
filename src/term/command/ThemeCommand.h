#pragma once
#include "MenuCommand.h"
#include "../Theme.h"

class ThemeCommand : public MenuCommand {
public:
    const char* label() const override { return "theme"; }
    void execute(IMenuHost& host) override { host.openSubMenu(this); }

    int         subCount()              const override { return Theme::COUNT; }
    const char* subLabel(int idx)       const override;
    bool        subIsActive(int idx)    const override;
    int         subItemH()              const override { return 18; }
    void onSubSelect(IMenuHost& host, int idx) override;
};
