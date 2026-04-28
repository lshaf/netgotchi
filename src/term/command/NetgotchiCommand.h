#pragma once
#include "MenuCommand.h"

class NetgotchiCommand : public MenuCommand {
public:
    const char* label()                    const override { return "netgotchi"; }
    void        execute(IMenuHost& host)         override { host.startNetgotchi(); }
};
