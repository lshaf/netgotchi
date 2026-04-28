#pragma once
#include "MenuCommand.h"

class NettrapCommand : public MenuCommand {
public:
    const char* label()                    const override { return "nettrap"; }
    void        execute(IMenuHost& host)         override { host.startNettrap(); }
};
