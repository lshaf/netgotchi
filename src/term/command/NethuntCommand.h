#pragma once
#include "MenuCommand.h"

class NethuntCommand : public MenuCommand {
public:
    const char* label()                    const override { return "nethunt"; }
    void        execute(IMenuHost& host)         override { host.startNethunt(); }
};
