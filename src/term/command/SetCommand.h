#pragma once
#include "MenuCommand.h"

class SetCommand : public MenuCommand {
public:
    const char* label()    const override { return "set"; }
    void execute(IMenuHost& host) override;

    int         subCount()              const override;
    const char* subLabel(int idx)       const override;
    bool        subIsActive(int idx)    const override;
    int         subItemH()              const override;
    const char* inputHint()             const override;
    void onSubSelect(IMenuHost& host, int idx) override;

private:
    enum class Section : uint8_t { Root, Theme, Brightness, DisplayOff };
    Section _section = Section::Root;
};
