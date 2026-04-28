#pragma once
#include "MenuCommand.h"

class PowerOffCommand : public MenuCommand {
public:
    const char* label() const override { return "poweroff"; }
    void execute(IMenuHost& host) override;
};
