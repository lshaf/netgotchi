#pragma once
#include "MenuCommand.h"

class NetguardCommand : public MenuCommand {
public:
    const char* label()                    const override { return "netguard"; }
    void        execute(IMenuHost& host)         override { host.startNetguard(); }
};
